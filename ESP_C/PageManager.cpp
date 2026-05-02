#include "PageManager.h"
#include <ESP8266WiFi.h>
#include <math.h>

// Forward declarations for functions from main
extern float comfortIndex(float t, float h);
extern void log(const String& tag, const String& msg);

struct ReplaceRule {
const char* from;
const char* to;
};

ReplaceRule rules[] = {
    {"Publish", "Pub"},
    {"Temperature", "Temp"},
    {"Humidity", "Hum"},
    {"Connected", "Conn"},
    {"Disconnected", "Disconn"},
    {"distance", "dist"},
    {"Distance", "Dist"},
    {"espC", "C"},
    {"espD", "D"},
    {"MQTT: OK", "MQTT: ok"},
    {"MQTT: FAIL", "MQTT: fail"},
    {" OK", ""},
    {" ", ""},
    {"Free", "F"},
    {"RSSI", ""},
};

PageManager::PageManager(LiquidCrystal_I2C& lcdRef, PubSubClient& mqttRef)
  : lcd(lcdRef), mqttClient(mqttRef) {
}

void PageManager::pageClock(float lastTemp, float lastHum) {
  struct tm timeinfo;
  char line1[17];
  char line2[17];

  // ===== LINE 1: TIME =====
  if (getLocalTime(&timeinfo)) {
    const char* days[] = {"CN", "Th2", "Th3", "Th4", "Th5", "Th6", "Th7"};

    snprintf(line1, sizeof(line1),
             "%s %02d:%02d %02d/%02d",
             days[timeinfo.tm_wday],
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1);
  } else {
    snprintf(line1, sizeof(line1), "No Time");
  }

  // ===== LINE 2: TEMP + HUM + COMFORT INDEX =====
  float ci = comfortIndex(lastTemp, lastHum);

  snprintf(line2, sizeof(line2),
           "%2.1f*C %2.0f%% %1.2f",
           lastTemp,
           lastHum,
           ci);

  // ===== DISPLAY =====
  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void PageManager::pageGreeting() {
  static int  enteredFromPage = -1;
  static char savedGreet[17]  = "";
  static char savedGreet2[17] = "";

  // Tính lại nếu vừa chuyển từ page khác sang Greeting
  if (lastPage != PAGE_GREETING) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int h = timeinfo.tm_hour;

      const char* greet;
      if      (h >= 5  && h < 7)  greet = "Early morning";
      else if (h >= 7  && h < 10) greet = "Active morning";
      else if (h >= 10 && h < 12) greet = "Almost noon";
      else if (h < 18)            greet = "Peaceful aftnoon";
      else                        greet = "Calm night";

      if (timeinfo.tm_wday == 0)
        greet = (h < 12) ? "Slow Sun morning" : "Relaxing Sunday";

      const char* msgs[] = {
        "don forget drink",
        "time to relaxing",
        "today is begin",
        "welcome Quang!"
      };

      strncpy(savedGreet,  greet,             16); savedGreet[16]  = '\0';
      strncpy(savedGreet2, msgs[random(0, 4)], 16); savedGreet2[16] = '\0';
    } else {
      strncpy(savedGreet,  "Xin chao :3",  16); savedGreet[16]  = '\0';
      strncpy(savedGreet2, "Tien Quang <3", 16); savedGreet2[16] = '\0';
    }
  }

  lcd.setCursor(0, 0); lcd.print(savedGreet);
  lcd.setCursor(0, 1); lcd.print(savedGreet2);
}

void PageManager::pageSystem() {
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1),
           "WiFi:%ddBm", WiFi.RSSI());

  snprintf(line2, sizeof(line2),
           "MQTT:%s",
           mqttClient.connected() ? "OK" : "FAIL");

  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2);
}

void PageManager::pageEvent() {
  lcd.setCursor(0, 0); lcd.print(eventLine1);
  lcd.setCursor(0, 1); lcd.print(eventLine2);
}

void PageManager::pageLog() {
  char line1[17];
  char line2[17];

  String logmsg = logMsg;
  for (auto &r : rules) logmsg.replace(r.from, r.to);

  String uptime = formatUptime(logTimestamp);
  snprintf(line1, sizeof(line1), "%s %s", uptime.c_str(), logTag.substring(0, 13).c_str());

  unsigned int start = (logmsg.length() > 16) ? (logmsg.length() - 16) : 0;
  String tail = logmsg.substring(start);
  strncpy(line2, tail.c_str(), 16);
  line2[16] = '\0';

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

String PageManager::formatUptime(unsigned long milliseconds) {
  unsigned long totalSeconds = milliseconds / 1000;
  unsigned long days = totalSeconds / 86400;
  unsigned long hours = (totalSeconds % 86400) / 3600;
  unsigned long seconds = totalSeconds % 3600;

  String result = "";
  if (days > 0) result += String(days) + "d";
  if (hours > 0) result += String(hours) + "h";
  result += String(seconds) + "s";

  return result;
}

void PageManager::showEvent(const String& l1, const String& l2, int& currentPage, unsigned long& lastLCDUpdate) {
  eventLine1 = l1.substring(0, 16); // giới hạn 16 ký tự LCD
  eventLine2 = l2.substring(0, 16);
  currentPage  = PAGE_EVENT;
  eventShownAt = millis();
  lastLCDUpdate = 0; // force render ngay lập tức
  lcd.clear();
  log("EVENT", l1 + " | " + l2);
}

void PageManager::setLog(const String& tag, const String& msg) {
  logTag = tag;
  logMsg = msg;
  logTimestamp = millis();
}

void PageManager::notifyTimeoutPage(int page) {
  if (page == PAGE_SYSTEM || page == PAGE_LOG || page == PAGE_EVENT) {
    eventShownAt = millis();
  } else {
    eventShownAt = 0;
  }
}

void PageManager::handleLCD(int& currentPage, unsigned long& lastLCDUpdate, const long LCD_INTERVAL, float lastTemp, float lastHum) {
  unsigned long now = millis();

  // --- Logic tự động chuyển page ---

  // EVENT: sau RETURN_TO_CLOCK_BY_EVENT ms (5s) về CLOCK
  if (currentPage == PAGE_EVENT
      && eventShownAt != 0
      && (now - eventShownAt >= RETURN_TO_CLOCK_BY_EVENT)) {
    currentPage   = PAGE_CLOCK;
    lastPageUpdate = now;
    eventShownAt  = 0;
    lcd.clear();
    lastPage = -1;
  }

  // SYSTEM & LOG: sau RETURN_TO_CLOCK ms (30s) về CLOCK
  if ((currentPage == PAGE_SYSTEM || currentPage == PAGE_LOG)
      && eventShownAt != 0
      && (now - eventShownAt >= RETURN_TO_CLOCK)) {
    currentPage   = PAGE_CLOCK;
    lastPageUpdate = now;
    eventShownAt  = 0;
    lcd.clear();
    lastPage = -1;
  }

  // CLOCK → GREETING sau 5s
  if (currentPage == PAGE_CLOCK
      && (now - lastPageUpdate >= PAGE_INTERVAL)) {
    currentPage    = PAGE_GREETING;
    lastPageUpdate = now;
    lcd.clear();
    lastPage = -1;
  }
  // GREETING → CLOCK sau 5s
  else if (currentPage == PAGE_GREETING
           && (now - lastPageUpdate >= PAGE_INTERVAL)) {
    currentPage    = PAGE_CLOCK;
    lastPageUpdate = now;
    lcd.clear();
    lastPage = -1;
  }

  // --- Throttle render ---
  if (now - lastLCDUpdate < 500) return; // render mỗi 0.5s (clock cần cập nhật phút)
  lastLCDUpdate = now;

  // Clear khi đổi page
  if (currentPage != lastPage) {
    lcd.clear();
  }

  switch (currentPage) {
    case PAGE_CLOCK:    pageClock(lastTemp, lastHum);    break;
    case PAGE_GREETING: pageGreeting(); break;
    case PAGE_SYSTEM:   pageSystem();   break;
    case PAGE_EVENT:    pageEvent();    break;
    case PAGE_LOG:      pageLog();      break;
  }

  lastPage = currentPage;
}