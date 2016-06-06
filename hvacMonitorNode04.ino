#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float tempMK; // Room temp
//int tempMKhighAlarm = 200;

char auth[] = "fromBlynkApp";

SimpleTimer timer;

WidgetLED led1(V18); // Heartbeat LED
WidgetTerminal terminal(V26); //Uptime reporting

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, "ssid", "pw");

  WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  sensors.begin();
  sensors.setResolution(10);

  timer.setInterval(2000L, sendTemps); // Temperature sensor polling interval
  heartbeatOn();
}

BLYNK_WRITE(V27) // App button to report uptime
{
  int pinData = param.asInt();

  if (pinData == 0)
  {
    timer.setTimeout(8000L, uptimeSend);
  }
}

void uptimeSend()  // Blinks a virtual LED in the Blynk app to show the ESP is live and reporting.
{
  float secDur = millis() / 1000;
  float minDur = secDur / 60;
  float hourDur = minDur / 60;
  terminal.println(String("Node04 (MK): ") + hourDur + " hours ");
  terminal.flush();
}

void heartbeatOn()  // Blinks a virtual LED in the Blynk app to show the ESP is live and reporting.
{
  led1.on();
  timer.setTimeout(2500L, heartbeatOff);
}

void heartbeatOff()
{
  led1.off();  // The OFF portion of the LED heartbeat indicator in the Blynk app
  timer.setTimeout(2500L, heartbeatOn);
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

  /*
    if (tempMK >= tempMKhighAlarm)
    {
      notifyAndOff();
    }
    */
}

void loop()
{
  Blynk.run();
  timer.run();
}

/*
// Input from Blynk app menu to select room temperature that triggers alarm
BLYNK_WRITE(V21) {
  switch (param.asInt())
  {
    case 1: { // Alarm Off
        tempMKhighAlarm = 200;
        break;
      }
    case 2: { // 80F Alarm
        tempMKhighAlarm = 80;
        break;
      }
    case 3: { // 81F Alarm
        tempMKhighAlarm = 81;
        break;
      }
    case 4: { // 82F Alarm;
        tempMKhighAlarm = 82;
        break;
      }
    case 5: { // 83F Alarm
        tempMKhighAlarm = 83;
        break;
      }
    case 6: { // 84F Alarm
        tempMKhighAlarm = 84;
        break;
      }
    default: {
        Serial.println("Unknown item selected");
      }
  }
}

void notifyAndOff()
{
  Blynk.notify(String("My room is ") + tempMK + "°F. Alarm disabled until reset."); // Send notification.
  Blynk.virtualWrite(V21, 1); // Rather than fancy timing, just disable alarm until reset.
  tempMKhighAlarm = 200;
}
*/
