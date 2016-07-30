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
bool sfSendFlag;           // Locks out sparkfun restart until complete
int atticTemp, bdrmTemp, keatonTemp, livTemp, outsideTemp, outsideTempPrev;
double tstatTemp;
int getWait = 2000;       // Duration to wait between syncs from Blynk

char auth[] = "fromBlynkApp";

// Sparkfun server
const char* hostSF = "data.sparkfun.com";
const char* streamId   = "publicKey";
const char* privateKey = "privateKey";

const char* ssid = "ssid";
const char* pw = "pw";

SimpleTimer timer;
WidgetTerminal terminal(V26);     //Uptime reporting
WidgetRTC rtc;
BLYNK_ATTACH_WIDGET(rtc, V8);

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pw);

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
  Serial.print("connecting to ");
  Serial.println(hostSF);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(hostSF, httpPort)) {
    Serial.println("connection failed");
    sfSendFlag = 0;
    return;
  }

  Serial.print("Requesting...");

  // This will send the request to the server
  client.print(String("GET ") + "/input/" + streamId + "?private_key=" + privateKey + "&attic=" + atticTemp + "&bdrm=" + tempMK + "&keaton=" + keatonTemp + "&liv=" + livTemp + "&outside=" + outsideTemp + "&tstat=" + tstatTemp + " HTTP/1.1\r\n" +
               "Host: " + hostSF + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 15000) {
      Serial.println(">>> Client Timeout !");
      sfSendFlag = 0;                                  // Trying this in hopes it will allow a restart in the event of timeout/failure.
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");

  sfSendFlag = 0;
}
