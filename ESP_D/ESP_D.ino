#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "DeviceManager.h"
#include "DHTManager.h"
#include "MqttManager.h"
#include "TouchManager.h"

// ================== TIME (NTP) ==================
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600;
const int   daylightOffset_sec = 0;

struct tm timeinfo;
bool timeReady = false;
char timeOfDay[32] = "";

// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== PIN ==================
#define MOTION_PIN D7

// ================== OBJECTS ==================
WiFiClient   espClient;
PubSubClient client(espClient);

// ================== MOTION ==================
bool lastMotionState = LOW;

// ================== RECONNECT ==================
unsigned long lastWifiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;
bool wifiReconnectRequested = false;

// ============================================================
//  LOG HELPER
// ============================================================
void log(const char* tag, const char* msg) {
  Serial.printf("[%lu] [%s] %s\n", millis(), tag, msg);
}

void logf(const char* tag, const char* fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  log(tag, buf);
}

// ============================================================
//  TIME HELPERS
// ============================================================
bool getLocalTimeNonBlocking(struct tm * info) {
  time_t now;
  time(&now);
  localtime_r(&now, info);
  return (info->tm_year > (1970 - 1900));
}

bool isNightTime() {
  if (!timeReady) return false;
  if (!getLocalTimeNonBlocking(&timeinfo)) return false;
  int hour = timeinfo.tm_hour, minute = timeinfo.tm_min;
  if (hour > 17 || (hour == 17 && minute >= 30)) return true;
  if (hour < 5) return true;
  return false;
}

void getTimeOfDay(char* outBuf, size_t bufLen) {
  if (!getLocalTimeNonBlocking(&timeinfo)) {
    strncpy(outBuf, "Unknown", bufLen - 1);
    outBuf[bufLen - 1] = '\0';
    return;
  }
  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  const char* greeting;
  if      (h >= 5  && h <= 10)                          greeting = "morning";
  else if (h >= 11 && h <= 13)                          greeting = "midday";
  else if (h >= 14 && (h < 17 || (h == 17 && m < 30))) greeting = "afternoon";
  else                                                  greeting = "evening";
  snprintf(outBuf, bufLen, "%02d:%02d Good %s", h, m, greeting);
}

void updateTimeOfDay() {
  if (!timeReady) return;
  char newState[32];
  getTimeOfDay(newState, sizeof(newState));
  if (strcmp(newState, timeOfDay) != 0) {
    strncpy(timeOfDay, newState, sizeof(timeOfDay) - 1);
    timeOfDay[sizeof(timeOfDay) - 1] = '\0';
    safePub("espD/time_of_day", timeOfDay);
    logf("TIME", "Updated: %s", timeOfDay);
  }
}

// ============================================================
//  WIFI
// ============================================================
void setup_wifi() {
  log("WIFI", "Starting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 15000) {
      log("WIFI", "Timeout! Continue without WiFi...");
      return;
    }
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "Connected. IP: %s", WiFi.localIP().toString().c_str());
  log("WIFI", buf);
}

// ============================================================
//  MQTT CALLBACK
// ============================================================
void callback(char* topic, byte* payload, unsigned int length) {
  char msg[64];
  unsigned int copyLen = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, copyLen);
  msg[copyLen] = '\0';

  logf("MQTT", "Received: %s = %s", topic, msg);

  if (strcmp(topic, "espD/relay1/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnRelay(0);
    else if (strcmp(msg, "OFF") == 0)    turnOffRelay(0);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(0);
  }
  else if (strcmp(topic, "espD/relay2/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnRelay(1);
    else if (strcmp(msg, "OFF") == 0)    turnOffRelay(1);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(1);
  }
  else if (strcmp(topic, "espD/servo1/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnServo(0);
    else if (strcmp(msg, "OFF") == 0)    turnOffServo(0);
    else if (strcmp(msg, "TOGGLE") == 0) toggleServo(0);
  }
  else if (strcmp(topic, "espD/servo2/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnServo(1);
    else if (strcmp(msg, "OFF") == 0)    turnOffServo(1);
    else if (strcmp(msg, "TOGGLE") == 0) toggleServo(1);
  }
  else if (strcmp(topic, "espD/fan/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnFan();
    else if (strcmp(msg, "OFF") == 0)    turnOffFan();
    else if (strcmp(msg, "TOGGLE") == 0) toggleFan();
  }
  else {
    logf("MQTT", "Unknown topic: %s", topic);
  }
}

// ============================================================
//  MOTION
// ============================================================
void handleMotion() {
  bool motion = digitalRead(MOTION_PIN);
  bool night  = isNightTime();

  if (motion != lastMotionState) {
    lastMotionState = motion;
    safePub("espD/motion", motion ? "1" : "0");
    logf("MOTION", "%s%s", motion ? "Detected" : "Cleared", night ? " (night)" : " (day)");

    if (motion == HIGH && night) {
      log("MOTION", "Auto-ON light (night + motion)");
      setLightAutoOn(true);
      setLight(true);
    } else if (motion == LOW && getLightAutoOn()) {
      log("MOTION", "Auto-OFF light (motion cleared)");
      setLightAutoOn(false);
      setLight(false);
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  log("BOOT", "ESP_D starting...");

  deviceBegin();
  pinMode(MOTION_PIN, INPUT);
  touchBegin();
  
  log("BOOT", "Pins initialized");

  setup_wifi();

  mqttBegin(callback);
  log("BOOT", "MQTT configured (buffer=512, keepalive=60s)");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  unsigned long start = millis();
  bool gotTime = false;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 10000) {
      log("TIME", "NTP sync timeout");
      break;
    }
  }
  if (getLocalTime(&timeinfo)) {
    gotTime = true;
  }
  timeReady = gotTime;

  if (timeReady) {
    char bootTime[32];
    getTimeOfDay(bootTime, sizeof(bootTime));
    logf("BOOT", "Time ready: %s", bootTime);
  } else {
    log("BOOT", "Time unavailable");
  }
  char heapBuf[64];
  snprintf(heapBuf, sizeof(heapBuf), "Setup complete. Free heap: %u", ESP.getFreeHeap());
  log("BOOT", heapBuf);

  dhtBegin();
  log("DHT", "DHT22 initialized");
}

// ============================================================
//  LOOP HELPERS
// ============================================================
void sub_loop_time() {
  static unsigned long lastCheck = 0;
  static unsigned long lastSync  = 0;

  if (millis() - lastCheck > 30000) {
    updateTimeOfDay();
    lastCheck = millis();
  }

  // Re-sync NTP mỗi 1 tiếng
  if (millis() - lastSync > 3600000) {
    log("TIME", "Re-syncing NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    lastSync = millis();
  }
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Log heap + WiFi RSSI mỗi 30s để phát hiện memory leak / signal yếu
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    lastHeapLog = millis();
    char heapBuf[128];
    snprintf(heapBuf, sizeof(heapBuf), "Free heap: %u bytes | WiFi RSSI: %d dBm | MQTT: %s", ESP.getFreeHeap(), WiFi.RSSI(), mqttIsConnected() ? "OK" : "DISCONNECTED");
    log("HEAP", heapBuf);
  }

  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    bool firstRetry = !wifiReconnectRequested;
    if (firstRetry || now - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      lastWifiReconnectAttempt = now;
      wifiReconnectRequested = true;
      log("WIFI", firstRetry ? "WiFi lost, starting reconnect..." : "WiFi reconnect retry...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
    return;
  }
  wifiReconnectRequested = false;

  if (!mqttIsConnected()) mqttReconnect();
  mqttLoop();

  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();
  handleDHT();
}
