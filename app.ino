#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Keypad.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include "sntp.h"
#include <ESP32Time.h>

unsigned long lastAlarmTime = 0;
unsigned long interval = 1000 * 10;

unsigned long lastWifiTime = 0;
unsigned long interval_Wifi = 1000 * 10;

DynamicJsonDocument doc(1024);

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

ESP32Time rtc(gmtOffset_sec);

const char* ssid = "STEM";
const char* password = "CreateAndBuild";

#define ROW_NUM 4     // four rows
#define COLUMN_NUM 3  // three columns

char keys[ROW_NUM][COLUMN_NUM] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

byte pin_rows[ROW_NUM] = { 12, 14, 27, 26 };
byte pin_column[COLUMN_NUM] = { 25, 33, 32 };

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

static const uint8_t PIN_MP3_TX = 16;
static const uint8_t PIN_MP3_RX = 17;

SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;

// 30 is the max volume
int volume = 30;

bool timeSet = false;
bool getAlarm = false;
int alarmStats = 70;

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%m %d %Y %H:%M:%S");

  time_t currentTime = mktime(&timeinfo);
  currentTime -= (8 * 3600);
  rtc.setTime(currentTime);

  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  timeSet = true;
}

void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}

void setup() {

  Serial.begin(9600);
  softwareSerial.begin(9600);

  if (player.begin(softwareSerial)) {
    Serial.println("Connected!\nDFPlayer is online!");
    player.volume(volume);

  } else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }

  WiFi.begin(ssid, password);
}

void loop() {

  char key = keypad.getKey();

  if (key) {
    // Serial.println(key);
    switch (key) {
      case '1':
        player.playFolder(1, 1);
        break;

      case '2':
        player.playFolder(1, 2);
        break;

      case '3':
        player.playFolder(1, 3);
        break;

      case '4':
        player.playFolder(1, 4);
        break;

      case '5':
        player.advertise(1);
        break;

      case '6':
        player.advertise(2);
        break;

      case '8':
        {
          int fileSize = player.readFileCountsInFolder(1);
          // Serial.println(fileSize);
          int calc = fileSize + 2;
          int randomNumber = random(1, calc);
          player.playFolder(1, randomNumber);
          break;
        }

      case '9':
        {
          if (player.readState() == 514) {
            player.start();
          } else {
            player.pause();
          }
          break;
        }

      case '7':
        player.advertise(3);
        break;

      case '0':
        player.stopAdvertise();
        break;

      case '*':
        player.volumeDown();
        break;

      case '#':
        player.volumeUp();
        break;
    }
  }

  if (millis() - lastWifiTime > interval_Wifi) {
    if ((WiFi.status() == WL_CONNECTED)) {

      if (!getAlarm) {
        player.playFolder(3, 1);
        sntp_set_time_sync_notification_cb(timeavailable);
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
        printLocalTime();
      }

      // Serial.print("Connected to the Wifi: ");
      // Serial.println(ssid);

      HTTPClient client;

      client.begin("https://raw.githubusercontent.com/FireFlyDeveloper/Client/main/alarm.json");
      int httpCode = client.GET();

      if (httpCode > 0) {

        getAlarm = true;
        String payload = client.getString();
        // Serial.println("Status code: " + String(httpCode));
        // Serial.println(payload);

        deserializeJson(doc, payload);

      } else {
        // Serial.println("There is an error in getting alarms!");
      }
    }

    lastWifiTime = millis();
  }

  if (getAlarm && timeSet) {
    if (millis() - lastAlarmTime > interval) {
      int currentHour24hr = rtc.getHour(true);  // Get current hour (24-hour format)
      int currentMinute = rtc.getMinute();      // Get current minute

      // Serial.print("currentHour: ");
      // Serial.print(currentHour24hr);
      // Serial.print(", currentMinute: ");
      // Serial.println(currentMinute);

      JsonArray alarm = doc["alarm"];
      for (JsonVariant value : alarm) {
        String alarmString = value.as<String>();
        int separatorIndex = alarmString.indexOf('|');
        String time = alarmString.substring(0, separatorIndex);
        int alarmValue = alarmString.substring(separatorIndex + 1).toInt();

        // Parse alarm hour and minute
        int alarmHour24hr = time.substring(0, time.indexOf(':')).toInt();
        int alarmMinute = time.substring(time.indexOf(':') + 1).toInt();

        // Serial.print("hr: ");
        // Serial.print(alarmHour24hr);
        // Serial.print(", min: ");
        // Serial.println(alarmMinute);

        // Compare alarm time with current time and trigger alarm only if it hasn't been triggered before
        if (alarmStats == 70 && currentHour24hr == alarmHour24hr && currentMinute == alarmMinute) {
          // Serial.println("Alarm!");
          // Trigger alarm action here

          if (player.readState() == 513) {
            player.advertise(alarmValue);
          } else {
            player.playFolder(2, alarmValue);
          }

          alarmStats = alarmMinute;
        }

        if (alarmStats != 70 && currentMinute != alarmStats) {
          // Serial.println("Alarm stats set to false!");
          alarmStats = 70;  // Reset alarm status once minute changes
        }
      }

      lastAlarmTime = millis();
    }
  }
}
