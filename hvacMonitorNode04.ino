/* Node04 responsibilities:
   - Send SparkFun attic, bdrm, KK, LK, outside, and tstat temps (picked up by analog.io) every minute.
   - Reports MK's bedroom temperature.
*/

#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#include <TimeLib.h>            // Used by WidgetRTC.h
#include <WidgetRTC.h>          // Blynk's RTC

#include <ESP8266mDNS.h>        // Required for OTA
#include <WiFiUdp.h>            // Required for OTA
#include <ArduinoOTA.h>         // Required for OTA

float tempMK;              // Room temp
bool sfSendFlag;
int atticTemp, bdrmTemp, keatonTemp, livTemp, outsideTemp, outsideTempPrev;
double tstatTemp;
int getWait = 2000;       // Duration to wait between syncs from Blynk

char auth[] = "fromBlynkApp";
char ssid[] = "ssid";
char pass[] = "pw";

// All sparkfun updates now handled by Blynk's WebHook widget
//const char* hostSF = "data.sparkfun.com";
//const char* streamId   = "publicKey";
//const char* privateKey = "privateKey";

SimpleTimer timer;
WidgetTerminal terminal(V26);     //Uptime reporting
WidgetRTC rtc;
BLYNK_ATTACH_WIDGET(rtc, V8);

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  sensors.begin();
  sensors.setResolution(10);

  // START OTA ROUTINE
  ArduinoOTA.setHostname("esp8266-Node04MK");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  // END OTA ROUTINE

  rtc.begin();

  timer.setInterval(2000L, sendTemps);    // Temperature sensor polling interval
  timer.setInterval(1000L, uptimeReport);
}

void loop()
{
  Blynk.run();
  timer.run();
  ArduinoOTA.handle();

  if (second() == 35 && sfSendFlag == 0)
  {
    sfSendFlag = 1;
    sfSync1();
  }
}

BLYNK_WRITE(V27) // App button to report uptime
{
  int pinData = param.asInt();

  if (pinData == 0)
  {
    timer.setTimeout(12000L, uptimeSend);
  }
}

void uptimeSend()
{
  long minDur = millis() / 60000L;
  long hourDur = millis() / 3600000L;
  if (minDur < 121)
  {
    terminal.print(String("Node04 (MK): ") + minDur + " mins @ ");
    terminal.println(WiFi.localIP());
  }
  else if (minDur > 120)
  {
    terminal.print(String("Node04 (MK): ") + hourDur + " hrs @ ");
    terminal.println(WiFi.localIP());
  }
  terminal.flush();
}

void uptimeReport() {
  if (second() > 4 && second() < 9)
  {
    Blynk.virtualWrite(104, minute());
  }
}

void sendTemps()
{
  sensors.requestTemperatures(); // Polls the sensors

  tempMK = sensors.getTempFByIndex(0); // Gets first probe on wire in lieu of by address

  if (tempMK > 0)
  {
    Blynk.virtualWrite(31, tempMK);
  }
  else
  {
    Blynk.virtualWrite(31, "ERR");
  }

  if (tempMK < 78) // Changes the color of the app's temperature readout based on temperature
  {
    Blynk.setProperty(V31, "color", "#04C0F8"); // Blue
  }
  else if (tempMK >= 78 && tempMK <= 80)
  {
    Blynk.setProperty(V31, "color", "#ED9D00"); // Yellow
  }
  else if (tempMK > 80)
  {
    Blynk.setProperty(V31, "color", "#D3435C"); // Red
  }

}

// START SPARKFUN UPDATE/REPORT PROCESS

void sfSync1() {
  Blynk.syncVirtual(V7);
  timer.setTimeout(getWait, sfSync2);
}

BLYNK_WRITE(V7) {
  atticTemp = param.asInt();
}

void sfSync2() {
  Blynk.syncVirtual(V4);
  timer.setTimeout(getWait, sfSync3);
}

BLYNK_WRITE(V4) {
  keatonTemp = param.asInt();
}


void sfSync3() {
  Blynk.syncVirtual(V6);
  timer.setTimeout(getWait, sfSync4);
}

BLYNK_WRITE(V6) {
  livTemp = param.asInt();
}


void sfSync4() {
  Blynk.syncVirtual(V12);

  // This is to screen out API errors of 0 degrees from being reported & messing up graph scale.
  outsideTempPrev = outsideTemp;

  if (outsideTemp = 0)
  {
    outsideTemp = outsideTempPrev;
  }

  timer.setTimeout(getWait, sfSync5);
}

BLYNK_WRITE(V12) {
  outsideTemp = param.asInt();
}


void sfSync5() {
  Blynk.syncVirtual(V3);
  timer.setTimeout(getWait, sfSend);
}

BLYNK_WRITE(V3) {
  tstatTemp = param.asDouble();
}


void sfSend()
{
  Blynk.virtualWrite(68, String("attic=") + atticTemp + "&bdrm=" + tempMK + "&keaton=" + keatonTemp + "&liv=" + livTemp + "&outside=" + outsideTemp + "&tstat=" + tstatTemp);

  sfSendFlag = 0;
}
