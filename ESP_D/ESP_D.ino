#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>

// ================== TIME (NTP) ==================
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600; // GMT+7
const int   daylightOffset_sec = 0;

struct tm timeinfo;
bool timeReady = false;

String timeOfDay = "";

// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";

// ================== CONSTANTS ==================
const int SERVO_OFF_ANGLE = 100;
const int SERVO_ON_ANGLE  = 180;

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
Servo servo1, servo2;

// ================== STATE ==================
bool relayState[2]  = {false, false};
bool servoState[2]  = {false, false};
bool lightState     = false;

unsigned long servoTimer[2] = {0, 0};
bool servoActive[2]         = {false, false};

// FIX 1: Dùng int thay vì bool để tránh lỗi ngầm với LOW/HIGH
const int LIGHT_ON  = LOW;   // D4 thường active-low (LOW = bật)
const int LIGHT_OFF = HIGH;

// ================== TOUCH STATE MACHINE ==================
enum TouchPhase {
  TOUCH_IDLE,
  TOUCH_COUNTING,
  TOUCH_HOLDING
};

TouchPhase    touchPhase     = TOUCH_IDLE;
bool          lastTouchState = LOW;
int           touchCount     = 0;
unsigned long touchStartTime = 0;
unsigned long lastTouchTime  = 0;

// ================== MOTION ==================
bool lastMotionState = LOW;

// ================== MQTT RECONNECT COOLDOWN ==================
// FIX 4: Thêm cooldown cho reconnect để tránh flood broker
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 giây

// ============================================================
//  TIME HELPERS
// ============================================================

bool isNightTime() {
  if (!timeReady) return false;
  if (!getLocalTime(&timeinfo)) return false;

  int hour   = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  // 17:30 → 23:59
  if (hour > 17 || (hour == 17 && minute >= 30)) return true;
  // 00:00 → 04:59
  if (hour < 5) return true;

  return false;
}

String getTimeOfDay() {
  if (!getLocalTime(&timeinfo)) return "Unknown";

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;

  String hStr = (h < 10 ? "0" : "") + String(h);
  String mStr = (m < 10 ? "0" : "") + String(m);

  String greeting;
  if      (h >= 5  && h <= 10)                               greeting = "morning";
  else if (h >= 11 && h <= 13)                               greeting = "midday";
  else if (h >= 14 && (h < 17 || (h == 17 && m < 30)))      greeting = "afternoon";
  else                                                       greeting = "evening";

  return hStr + ":" + mStr + " Good " + greeting;
}

void updateTimeOfDay() {
  if (!timeReady) return;

  String newState = getTimeOfDay();
  if (newState != timeOfDay) {
    timeOfDay = newState;
    client.publish("espD/time_of_day", timeOfDay.c_str());
  }
}

// ============================================================
//  WIFI
// ============================================================

void setup_wifi() {
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - start > 10000) break;
  }
}

// ============================================================
//  RELAY
// ============================================================

void setRelay(int idx, bool state) {
  relayState[idx] = state;

  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);

  String topic = "espD/relay" + String(idx + 1) + "/state";
  client.publish(topic.c_str(), state ? "ON" : "OFF");
}

void toggleRelay(int idx) { setRelay(idx, !relayState[idx]); }
void turnOnRelay(int idx)  { setRelay(idx, true);             }
void turnOffRelay(int idx) { setRelay(idx, false);            }

// ============================================================
//  SERVO
// ============================================================

void handleServoTimeout() {
  for (int i = 0; i < 2; i++) {
    if (servoActive[i] && millis() - servoTimer[i] > 250) {
      if (i == 0) servo1.detach();
      else        servo2.detach();
      servoActive[i] = false;
    }
  }
}

void setServo(int idx, bool state) {
  if (servoState[idx] == state) return;

  servoState[idx] = state;
  int angle = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

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
  client.publish(topic.c_str(), state ? "ON" : "OFF");
}

void toggleServo(int idx) { setServo(idx, !servoState[idx]); }
void turnOnServo(int idx)  { setServo(idx, true);             }
void turnOffServo(int idx) { setServo(idx, false);            }

// ============================================================
//  LIGHT
// ============================================================

// FIX 2: Thêm guard để tránh spam MQTT khi trạng thái không đổi
void setLight(bool state) {
  if (lightState == state) return; // Không làm gì nếu trạng thái đã đúng
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? LIGHT_ON : LIGHT_OFF);
  client.publish("espD/light/state", state ? "ON" : "OFF");
}

// ============================================================
//  SHUTDOWN ALL
// ============================================================

void shutdownAllDevices() {
  turnOffRelay(0);
  turnOffRelay(1);
  turnOffServo(0);
  turnOffServo(1);

  client.publish("espC/relay1/set", "OFF");
  client.publish("espC/relay2/set", "OFF");
  client.publish("espC/relay3/set", "OFF");
}

// ============================================================
//  MQTT CALLBACK
// ============================================================

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  String t = String(topic);

  if (t == "espD/relay1/set") {
    if      (msg == "ON")     turnOnRelay(0);
    else if (msg == "OFF")    turnOffRelay(0);
    else if (msg == "TOGGLE") toggleRelay(0);
  }

  if (t == "espD/relay2/set") {
    if      (msg == "ON")     turnOnRelay(1);
    else if (msg == "OFF")    turnOffRelay(1);
    else if (msg == "TOGGLE") toggleRelay(1);
  }

  if (t == "espD/servo1/set") {
    if      (msg == "ON")     turnOnServo(0);
    else if (msg == "OFF")    turnOffServo(0);
    else if (msg == "TOGGLE") toggleServo(0);
  }

  if (t == "espD/servo2/set") {
    if      (msg == "ON")     turnOnServo(1);
    else if (msg == "OFF")    turnOffServo(1);
    else if (msg == "TOGGLE") toggleServo(1);
  }
}

// ============================================================
//  MQTT RECONNECT
// ============================================================

// FIX 4: Thêm cooldown 5s giữa các lần reconnect
void reconnect() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    client.publish("espD/status", "online", true);

    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");
  }
}

// ============================================================
//  MOTION
// ============================================================

// FIX 3: Chỉ tắt đèn khi không có motion — không tắt nếu đang bật thủ công
// Logic mới: đèn tự động chỉ BẬT khi có motion + ban đêm
//            TẮT khi hết motion (bất kể thời gian) — chỉ khi lightState=true và không phải bật thủ công
// Để phân biệt "bật tự động" vs "bật thủ công", thêm flag riêng
bool lightAutoOn = false; // đèn đang bật do motion tự động

void handleMotion() {
  bool motion = digitalRead(MOTION_PIN);
  bool night  = isNightTime();

  if (motion != lastMotionState) {
    lastMotionState = motion;
    client.publish("espD/motion", motion ? "1" : "0");

    if (motion == HIGH && night) {
      // Có người + ban đêm → bật đèn tự động
      lightAutoOn = true;
      setLight(true);
    } else if (motion == LOW && lightAutoOn) {
      // Hết motion, đèn đang bật tự động → tắt
      lightAutoOn = false;
      setLight(false);
    }
    // Nếu motion=HIGH ban ngày, hoặc motion=LOW mà đèn bật thủ công → không làm gì
  }
}

// ============================================================
//  TOUCH — STATE MACHINE (non-blocking)
//
//  Cảm biến chạm: HIGH = đang chạm, LOW = đã thả
//  pinMode: INPUT (không dùng PULLUP)
//
//  Sơ đồ trạng thái:
//
//   IDLE ──[chạm: LOW→HIGH]──► COUNTING
//            (touchCount = 1)
//
//   COUNTING ──[chạm lại trong 400ms]──► COUNTING
//               (touchCount++)
//
//   COUNTING ──[giữ > 2000ms, touchCount == 1]──► HOLDING
//               → shutdownAllDevices()
//
//   COUNTING ──[thả + 400ms không chạm thêm]──► IDLE
//               → thực thi hành động theo touchCount
//
//   HOLDING ──[thả: HIGH→LOW]──► IDLE
// ============================================================

void handleTouch() {
  bool currentState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // ===== Phát hiện chạm (cạnh lên: LOW → HIGH) =====
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && now - lastTouchTime < 400) {
      touchCount++;
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
    }

    lastTouchTime = now;
  }

  // FIX 5: Kiểm tra HOLD — thêm guard touchPhase != TOUCH_HOLDING để tránh gọi lặp
  if (currentState == HIGH
      && touchPhase == TOUCH_COUNTING
      && touchCount == 1
      && now - touchStartTime > 2000) {

    touchPhase = TOUCH_HOLDING; // chuyển trạng thái TRƯỚC khi gọi action
    touchCount = 0;
    shutdownAllDevices();
  }

  // ===== Phát hiện thả tay (cạnh xuống: HIGH → LOW) =====
  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      touchPhase = TOUCH_IDLE;
    }
  }

  // ===== Timeout multi-tap: 400ms sau lần chạm cuối → thực thi =====
  if (touchPhase == TOUCH_COUNTING
      && currentState == LOW
      && now - lastTouchTime > 400) {

    if      (touchCount == 1) toggleServo(0);
    else if (touchCount == 2) toggleServo(1);
    else if (touchCount == 3) toggleRelay(0);

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN,  OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(TOUCH_PIN,  INPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN,  LIGHT_OFF); // Dùng constant thay vì LOW cho nhất quán

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  unsigned long start = millis();
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    if (millis() - start > 10000) break;
  }

  timeReady = true;
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

  if (millis() - lastSync > 3600000) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    lastSync = millis();
  }
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();
}
