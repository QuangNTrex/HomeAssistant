/*
 * ESP_D — Fixed Version
 * Fixes:
 *  1. Servo: attach/write/detach đúng cách, detach ngay sau khi servo đã đến vị trí
 *     (dùng timer 300ms thay vì 250ms, và KHÔNG attach lại nếu đang active)
 *  2. reconnect() non-blocking với cooldown 5s
 *  3. MQTT buffer tăng lên 512 bytes
 *  4. setKeepAlive(60) để tránh broker ngắt kết nối
 *  5. WiFi watchdog trong loop()
 *  6. Serial logging đầy đủ để debug
 *  7. Guard state cho relay/servo tránh spam MQTT
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>

// ================== TIME (NTP) ==================
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600;
const int   daylightOffset_sec = 0;

struct tm timeinfo;
bool timeReady = false;
String timeOfDay = "";

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

// ================== OBJECTS ==================
WiFiClient   espClient;
PubSubClient client(espClient);
Servo        servo1, servo2;

// ================== STATE ==================
bool relayState[2] = {false, false};
bool servoState[2] = {false, false};
bool lightState    = false;
bool lightAutoOn   = false;

unsigned long servoTimer[2]  = {0, 0};
bool          servoActive[2] = {false, false};

const int LIGHT_ON  = LOW;
const int LIGHT_OFF = HIGH;

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

// ============================================================
//  LOG HELPER
// ============================================================
void log(const String& tag, const String& msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(msg);
}

// ============================================================
//  MQTT PUBLISH HELPER
// ============================================================
bool safePub(const char* topic, const char* payload, bool retained = false) {
  if (!client.connected()) {
    log("MQTT", "Publish SKIPPED (disconnected): " + String(topic));
    return false;
  }
  bool ok = client.publish(topic, payload, retained);
  log("MQTT", "Publish " + String(topic) + " = " + String(payload) + (ok ? " OK" : " FAIL"));
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

String getTimeOfDay() {
  if (!getLocalTime(&timeinfo)) return "Unknown";
  int h = timeinfo.tm_hour, m = timeinfo.tm_min;
  String hStr = (h < 10 ? "0" : "") + String(h);
  String mStr = (m < 10 ? "0" : "") + String(m);
  String greeting;
  if      (h >= 5  && h <= 10)                          greeting = "morning";
  else if (h >= 11 && h <= 13)                          greeting = "midday";
  else if (h >= 14 && (h < 17 || (h == 17 && m < 30))) greeting = "afternoon";
  else                                                  greeting = "evening";
  return hStr + ":" + mStr + " Good " + greeting;
}

void updateTimeOfDay() {
  if (!timeReady) return;
  String newState = getTimeOfDay();
  if (newState != timeOfDay) {
    timeOfDay = newState;
    safePub("espD/time_of_day", timeOfDay.c_str());
    log("TIME", "Updated: " + timeOfDay);
  }
}

// ============================================================
//  WIFI
// ============================================================
void setup_wifi() {
  log("WIFI", "Connecting to: " + String(ssid));
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 15000) {
      log("WIFI", "Timeout! Continuing without WiFi...");
      return;
    }
  }
  log("WIFI", "Connected. IP: " + WiFi.localIP().toString());
}

// ============================================================
//  RELAY
// ============================================================
void setRelay(int idx, bool state) {
  if (relayState[idx] == state) {
    log("RELAY", "relay" + String(idx+1) + " already " + (state ? "ON" : "OFF") + ", skip");
    return;
  }
  relayState[idx] = state;
  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);
  log("RELAY", "relay" + String(idx+1) + " → " + (state ? "ON" : "OFF"));

  String topic = "espD/relay" + String(idx + 1) + "/state";
  safePub(topic.c_str(), state ? "ON" : "OFF", true);
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
    log("SERVO", "servo" + String(idx+1) + " already " + (state ? "ON" : "OFF") + ", skip");
    return;
  }

  // FIX: Nếu servo đang trong quá trình di chuyển, chờ detach xong mới cho phép lệnh mới
  if (servoActive[idx]) {
    log("SERVO", "servo" + String(idx+1) + " busy (moving), command queued/ignored");
    return;
  }

  servoState[idx]  = state;
  int angle        = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

  log("SERVO", "servo" + String(idx+1) + " → " + (state ? "ON" : "OFF") + " (angle=" + String(angle) + ")");

  if (idx == 0) {
    servo1.attach(SERVO1_PIN);
    servo1.write(angle);
  } else {
    servo2.attach(SERVO2_PIN);
    servo2.write(angle);
  }

  servoActive[idx] = true;
  servoTimer[idx]  = millis();

  String topic = "espD/servo" + String(idx + 1) + "/state";
  safePub(topic.c_str(), state ? "ON" : "OFF", true);
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
      log("SERVO", "servo" + String(i+1) + " detached (movement complete)");
    }
  }
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

  safePub("espC/relay1/set", "OFF");
  safePub("espC/relay2/set", "OFF");
  safePub("espC/relay3/set", "OFF");
  log("SYSTEM", "Shutdown complete");
}

// ============================================================
//  MQTT CALLBACK
// ============================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t = String(topic);
  log("MQTT", "Received: " + t + " = " + msg);

  if (t == "espD/relay1/set") {
    if      (msg == "ON")     turnOnRelay(0);
    else if (msg == "OFF")    turnOffRelay(0);
    else if (msg == "TOGGLE") toggleRelay(0);
  }
  else if (t == "espD/relay2/set") {
    if      (msg == "ON")     turnOnRelay(1);
    else if (msg == "OFF")    turnOffRelay(1);
    else if (msg == "TOGGLE") toggleRelay(1);
  }
  else if (t == "espD/servo1/set") {
    if      (msg == "ON")     turnOnServo(0);
    else if (msg == "OFF")    turnOffServo(0);
    else if (msg == "TOGGLE") toggleServo(0);
  }
  else if (t == "espD/servo2/set") {
    if      (msg == "ON")     turnOnServo(1);
    else if (msg == "OFF")    turnOffServo(1);
    else if (msg == "TOGGLE") toggleServo(1);
  }
  else {
    log("MQTT", "Unknown topic: " + t);
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

  log("MQTT", "Attempting reconnect to " + String(mqtt_server) + "...");

  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    client.publish("espD/status", "online", true);
    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");
    log("MQTT", "Subscribed to all topics");
  } else {
    log("MQTT", "Failed, rc=" + String(client.state()) + " — retry in " + String(RECONNECT_INTERVAL/1000) + "s");
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
    log("MOTION", motion ? "Detected" : "Cleared" + String(night ? " (night)" : " (day)"));

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
      log("TOUCH", "Multi-tap count: " + String(touchCount));
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

    log("TOUCH", "Execute tap action, count=" + String(touchCount));
    if      (touchCount == 1) { toggleServo(0); log("TOUCH", "1 tap → toggleServo1 (den chinh)"); }
    else if (touchCount == 2) { toggleServo(1); log("TOUCH", "2 tap → toggleServo2 (den bep)"); }
    else if (touchCount == 3) { toggleRelay(0); log("TOUCH", "3 tap → toggleRelay1"); }
    else { log("TOUCH", "No action for count=" + String(touchCount)); }

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
  pinMode(MOTION_PIN, INPUT);
  pinMode(TOUCH_PIN,  INPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN,  LIGHT_OFF);
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
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 10000) {
      log("TIME", "NTP sync timeout");
      break;
    }
  }
  timeReady = true;
  log("BOOT", "Time ready: " + getTimeOfDay());
  log("BOOT", "Setup complete. Free heap: " + String(ESP.getFreeHeap()));
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
    log("HEAP", "Free heap: " + String(ESP.getFreeHeap()) + " bytes | WiFi RSSI: " + String(WiFi.RSSI()) + " dBm | MQTT: " + (client.connected() ? "OK" : "DISCONNECTED"));
  }

  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) {
    log("WIFI", "Disconnected! Reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!client.connected()) reconnect();
  client.loop();

  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();
}
