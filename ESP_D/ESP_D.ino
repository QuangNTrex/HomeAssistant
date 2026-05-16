#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>
#include <DHT.h>

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

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";
const int   mqtt_port   = 1883;

// ================== CONSTANTS ==================
const int SERVO_OFF_ANGLE = 100;
const int SERVO_ON_ANGLE  = 180;

// FIX: Tăng thời gian chờ servo di chuyển từ 250ms → 400ms
// Servo thường cần 300-400ms để đi từ 100° → 180°
const unsigned long SERVO_DETACH_DELAY = 400;

// ================== PIN ==================
#define RELAY1_PIN D1
#define RELAY2_PIN D2
#define SERVO1_PIN D5
#define SERVO2_PIN D6
#define MOTION_PIN D7
#define TOUCH_PIN  D0
#define LIGHT_PIN  D4
#define FAN_PIN    D8
#define DHT_PIN    D3
#define DHT_TYPE   DHT22

DHT dht(DHT_PIN, DHT_TYPE);

float temperature = 0;
float humidity = 0;
const float DOWN_HUMI = 10.0; // Giảm 10% độ ẩm

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

// ================== OBJECTS ==================
WiFiClient   espClient;
PubSubClient client(espClient);
Servo        servo1, servo2;

// ================== STATE ==================
bool relayState[2] = {false, false};
bool servoState[2] = {false, false};
bool lightState    = false;
bool lightAutoOn   = false;
bool fanState      = false;

unsigned long servoTimer[2]  = {0, 0};
bool          servoActive[2] = {false, false};

const int LIGHT_ON      = LOW;
const int LIGHT_OFF     = HIGH;
const int FAN_ON_SPEED  = 1024;
const int FAN_OFF_SPEED = 0;

// ================== TOUCH STATE MACHINE ==================
enum TouchPhase { TOUCH_IDLE, TOUCH_COUNTING, TOUCH_HOLDING };
TouchPhase    touchPhase     = TOUCH_IDLE;
bool          lastTouchState = LOW;
int           touchCount     = 0;
unsigned long touchStartTime = 0;
unsigned long lastTouchTime  = 0;

// ================== MOTION ==================
bool lastMotionState = LOW;

// ================== RECONNECT ==================
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;
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
//  MQTT PUBLISH HELPER
// ============================================================
bool safePub(const char* topic, const char* payload, bool retained = false) {
  if (!client.connected()) {
    char buf[96];
    snprintf(buf, sizeof(buf), "Publish SKIPPED (disconnected): %s", topic);
    log("MQTT", buf);
    return false;
  }

  bool ok = client.publish(topic, payload, retained);
  char buf[128];
  snprintf(buf, sizeof(buf), "Publish %s = %s %s", topic, payload, ok ? "OK" : "FAIL");
  log("MQTT", buf);
  return ok;
}

// ============================================================
//  TIME HELPERS
// ============================================================
bool isNightTime() {
  if (!timeReady) return false;
  if (!getLocalTime(&timeinfo)) return false;
  int hour = timeinfo.tm_hour, minute = timeinfo.tm_min;
  if (hour > 17 || (hour == 17 && minute >= 30)) return true;
  if (hour < 5) return true;
  return false;
}

void getTimeOfDay(char* outBuf, size_t bufLen) {
  if (!getLocalTime(&timeinfo)) {
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

// ============================================================
//  HEAT INDEX & COMFORT INDEX CALCULATIONS
// ============================================================
float computeHeatIndex(float t_c, float humidity) {
  // 1. Chuyển đổi sang độ F
  float t = (t_c * 1.8) + 32.0;
  float hi;

  // 2. Tính toán Heat Index dựa trên ngưỡng 80 độ F (26.7 độ C)
  if (t < 80.0) {
    // Công thức đơn giản cho nhiệt độ thấp
    hi = 0.5 * (t + 61.0 + ((t - 68.0) * 1.2) + (humidity * 0.094));
  } 
  else {
    // Công thức hồi quy Rothfusz đầy đủ
    hi = -42.379 + (2.04901523 * t) + (10.14333127 * humidity) 
         - (0.22475541 * t * humidity) - (0.00683783 * t * t) 
         - (0.05481717 * humidity * humidity) + (0.00122874 * t * t * humidity) 
         + (0.00085282 * t * humidity * humidity) - (0.00000199 * t * t * humidity * humidity);

    // Hiệu chỉnh 1: Nếu độ ẩm thấp (< 13%) và nhiệt độ từ 80-112 độ F
    if ((humidity < 13.0) && (t >= 80.0) && (t <= 112.0)) {
      float adj = ((13.0 - humidity) / 4.0) * sqrt((17.0 - abs(t - 95.0)) / 17.0);
      hi -= adj;
    } 
    // Hiệu chỉnh 2: Nếu độ ẩm cao (> 85%) và nhiệt độ từ 80-87 độ F
    else if ((humidity > 85.0) && (t >= 80.0) && (t <= 87.0)) {
      float adj = ((humidity - 85.0) / 10.0) * ((87.0 - t) / 5.0);
      hi += adj;
    }
  }

  // 3. Chuyển đổi kết quả ngược lại độ C
  return (hi - 32.0) / 1.8;
}

// float computeHeatIndex(float t, float h, float v) {
//   // vapor pressure (e)
//   float e = (h / 100.0) * 6.105 * exp((17.27 * t) / (237.7 + t));

//   // Apparent Temperature (Steadman)
//   float at = t + 0.33 * e - 0.70 * v - 4.0;

//   return at;
// }

float comfortIndex(float t, float h) {
  float cool    = 10.0;
  float comfort = 25.0;
  float hot     = 45.0;

  // chỉ dùng Heat Index khi đủ điều kiện
  float base = (t >= 15 && h >= 40) ? computeHeatIndex(t, h) : t;

  float ci;

  // ❄️ Lạnh
  if (base <= cool) {
    ci = 5.0 * (base - cool) / (comfort - cool);
  }

  // 🌤️ Mát → dễ chịu
  else if (base <= comfort) {
    ci = 5.0 * (base - cool) / (comfort - cool);
  }

  // 🔥 Nóng
  else if (base <= hot) {
    ci = 5.0 + 5.0 * (base - comfort) / (hot - comfort);
  }

  // 🔴 Rất nóng
  else {
    ci = 5.0 + 5.0 * (base - comfort) / (hot - comfort);
  }

  return ci;
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
//  RELAY
// ============================================================
void setRelay(int idx, bool state) {
  if (relayState[idx] == state) {
    logf("RELAY", "relay%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }
  relayState[idx] = state;
  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);
  logf("RELAY", "relay%d → %s", idx + 1, state ? "ON" : "OFF");

  char topic[24];
  snprintf(topic, sizeof(topic), "espD/relay%d/state", idx + 1);
  safePub(topic, state ? "ON" : "OFF", true);
}

void toggleRelay(int idx)  { setRelay(idx, !relayState[idx]); }
void turnOnRelay(int idx)  { setRelay(idx, true);  }
void turnOffRelay(int idx) { setRelay(idx, false); }

// ============================================================
//  SERVO
// ============================================================
/*
  Servo flow:
  1. attach(pin)
  2. write(angle)
  3. Chờ SERVO_DETACH_DELAY ms để servo có thể đến vị trí
  4. detach() — tránh jitter và giải phóng PWM
  
  FIX: Nếu đang trong quá trình di chuyển (servoActive=true), 
  không attach/write lại để tránh conflict.
*/
void setServo(int idx, bool state) {
  if (servoState[idx] == state) {
    logf("SERVO", "servo%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }

  // FIX: Nếu servo đang trong quá trình di chuyển, chờ detach xong mới cho phép lệnh mới
  if (servoActive[idx]) {
    logf("SERVO", "servo%d busy (moving), command queued/ignored", idx + 1);
    return;
  }

  servoState[idx]  = state;
  int angle        = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

  logf("SERVO", "servo%d → %s (angle=%d)", idx + 1, state ? "ON" : "OFF", angle);

  if (idx == 0) {
    servo1.attach(SERVO1_PIN);
    servo1.write(angle);
  } else {
    servo2.attach(SERVO2_PIN);
    servo2.write(angle);
  }

  servoActive[idx] = true;
  servoTimer[idx]  = millis();

  char topic[24];
  snprintf(topic, sizeof(topic), "espD/servo%d/state", idx + 1);
  safePub(topic, state ? "ON" : "OFF", true);
}

void toggleServo(int idx) { setServo(idx, !servoState[idx]); }
void turnOnServo(int idx)  { setServo(idx, true);  }
void turnOffServo(int idx) { setServo(idx, false); }

// FIX: Tăng delay detach lên 400ms + log
void handleServoTimeout() {
  for (int i = 0; i < 2; i++) {
    if (servoActive[i] && (millis() - servoTimer[i] > SERVO_DETACH_DELAY)) {
      if (i == 0) servo1.detach();
      else        servo2.detach();
      servoActive[i] = false;
      logf("SERVO", "servo%d detached (movement complete)", i + 1);
    }
  }
}
//////////////////////////////////////////
void handleDHT() {
  if (millis() - lastDHTRead < DHT_INTERVAL) return;
  lastDHTRead = millis();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    log("DHT", "Read failed");
    return;
  }

  temperature = t;
  humidity = h;

  char tempStr[8];
  char humStr[8];
  snprintf(tempStr, sizeof(tempStr), "%.1f", t);
  snprintf(humStr, sizeof(humStr), "%.1f", h);

  safePub("espD/temp", tempStr, true);
  safePub("espD/hum", humStr, true);

  // Tính toán và publish Heat Index và Comfort Index
  float hi = computeHeatIndex(t, h);
  float ci = comfortIndex(t, h);

  char hiStr[8];
  char ciStr[8];
  snprintf(hiStr, sizeof(hiStr), "%.2f", hi);
  snprintf(ciStr, sizeof(ciStr), "%.2f", ci);

  safePub("espD/heat_index", hiStr, true);
  safePub("espD/comfort_index", ciStr, true);

  logf("DHT", "T=%s°C H=%s%% HI=%s CI=%s", tempStr, humStr, hiStr, ciStr);
}

// ============================================================
//  LIGHT
// ============================================================
void setLight(bool state) {
  if (lightState == state) return;
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? LIGHT_ON : LIGHT_OFF);
  safePub("espD/light/state", state ? "ON" : "OFF", true);
  log("LIGHT", state ? "ON" : "OFF");
}

void setFan(bool state) {
  if (fanState == state) {
    logf("FAN", "fan already %s, skip", state ? "ON" : "OFF");
    return;
  }

  fanState = state;
  int speed = state ? FAN_ON_SPEED : FAN_OFF_SPEED;
  analogWrite(FAN_PIN, speed);
  safePub("espD/fan/state", state ? "ON" : "OFF", true);
  safePub("espD/fan/speed", state ? "1024" : "0", true);
  logf("FAN", "fan → %s (speed=%d)", state ? "ON" : "OFF", speed);
}

void toggleFan() { setFan(!fanState); }
void turnOnFan()  { setFan(true); }
void turnOffFan() { setFan(false); }

// ============================================================
//  SHUTDOWN ALL
// ============================================================
void shutdownAllDevices() {
  log("SYSTEM", "SHUTDOWN ALL devices triggered");
  turnOffRelay(0);
  turnOffRelay(1);
  turnOffServo(0);
  turnOffServo(1);
  setLight(false);
  setFan(false);

  safePub("espC/relay1/set", "OFF");
  safePub("espC/relay2/set", "OFF");
  safePub("espC/relay3/set", "OFF");
  safePub("espD/fan/set", "OFF");
  log("SYSTEM", "Shutdown complete");
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
//  MQTT RECONNECT (NON-BLOCKING)
// ============================================================
void reconnect() {
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  char buf[96];
  snprintf(buf, sizeof(buf), "Attempting reconnect to %s...", mqtt_server);
  log("MQTT", buf);

  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    client.publish("espD/status", "online", true);
    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");
    client.subscribe("espD/fan/set");
    log("MQTT", "Subscribed to all topics");
  } else {
    snprintf(buf, sizeof(buf), "Failed, rc=%d — retry in %lu s", client.state(), RECONNECT_INTERVAL / 1000);
    log("MQTT", buf);
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
      lightAutoOn = true;
      setLight(true);
    } else if (motion == LOW && lightAutoOn) {
      log("MOTION", "Auto-OFF light (motion cleared)");
      lightAutoOn = false;
      setLight(false);
    }
  }
}

// ============================================================
//  TOUCH STATE MACHINE (NON-BLOCKING)
// ============================================================
void handleTouch() {
  bool currentState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // Phát hiện chạm (cạnh lên: LOW → HIGH)
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && (now - lastTouchTime < 400)) {
      touchCount++;
      logf("TOUCH", "Multi-tap count: %d", touchCount);
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
      log("TOUCH", "New tap, count: 1");
    }
    lastTouchTime = now;
  }

  // Kiểm tra HOLD (giữ > 2s, single tap)
  if (currentState == HIGH
      && touchPhase == TOUCH_COUNTING
      && touchCount == 1
      && (now - touchStartTime > 2000)) {

    log("TOUCH", "HOLD detected → shutdownAllDevices");
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
    shutdownAllDevices();
  }

  // Phát hiện thả tay (cạnh xuống: HIGH → LOW)
  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      log("TOUCH", "Released from HOLD → IDLE");
      touchPhase = TOUCH_IDLE;
    }
  }

  // Timeout multi-tap: 400ms sau lần chạm cuối → thực thi
  if (touchPhase == TOUCH_COUNTING
      && currentState == LOW
      && (now - lastTouchTime > 400)) {

    logf("TOUCH", "Execute tap action, count=%d", touchCount);
    if      (touchCount == 1) { toggleServo(0); log("TOUCH", "1 tap → toggleServo1 (den chinh)"); }
    else if (touchCount == 2) { toggleServo(1); log("TOUCH", "2 tap → toggleServo2 (den bep)"); }
    else if (touchCount == 3) { toggleRelay(0); log("TOUCH", "3 tap → toggleRelay1"); }
    else { logf("TOUCH", "No action for count=%d", touchCount); }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  log("BOOT", "ESP_D starting...");

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN,  OUTPUT);
  pinMode(FAN_PIN,    OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(TOUCH_PIN,  INPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN,  LIGHT_OFF);
  analogWrite(FAN_PIN,    FAN_OFF_SPEED);
  log("BOOT", "Pins initialized");

  setup_wifi();

  // FIX: Tăng buffer MQTT lên 512 bytes
  client.setBufferSize(512);
  // FIX: KeepAlive 60s để broker không ngắt kết nối khi xử lý servo/relay
  client.setKeepAlive(60);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
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

  dht.begin();
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
    snprintf(heapBuf, sizeof(heapBuf), "Free heap: %u bytes | WiFi RSSI: %d dBm | MQTT: %s", ESP.getFreeHeap(), WiFi.RSSI(), client.connected() ? "OK" : "DISCONNECTED");
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

  if (!client.connected()) reconnect();
  client.loop();

  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();
  handleDHT();
}
