#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>

// ================== TIME (NTP) ==================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // GMT+7
const int   daylightOffset_sec = 0;

struct tm timeinfo;
bool timeReady = false;

// ============
String timeOfDay = "";

// ================== WIFI ==================
const char* ssid = "Test";
const char* password = "24082002";

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";

// =================== Constant
const int SERVO_OFF_ANGLE = 110;
const int SERVO_ON_ANGLE  = 180;

// ================== PIN ==================
#define SERVO1_PIN D1
#define SERVO2_PIN D2
#define RELAY1_PIN D5
#define RELAY2_PIN D6
#define MOTION_PIN D7
#define TOUCH_PIN  D0
#define LIGHT_PIN  D3   // đèn

// ================== OBJECT ==================
WiFiClient espClient;
PubSubClient client(espClient);
Servo servo1, servo2;

// ================== STATE ==================
bool relayState[2] = {false, false};
bool servoState[2] = {false, false}; // false=0°, true=90°
bool lightState = false;

// touch
unsigned long lastTouchTime = 0;
bool lastTouchState = HIGH;
int touchCount = 0;

// motion
bool lastMotionState = LOW;

//=========
bool isNightTime() {
  if (!timeReady) return false;

  if (!getLocalTime(&timeinfo)) return false;

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  // 17:30 → 23:59
  if (hour > 17 || (hour == 17 && minute >= 30)) {
    return true;
  }

  // 00:00 → 04:59
  if (hour < 5) {
    return true;
  }

  return false;
}

String getTimeOfDay() {
  if (!getLocalTime(&timeinfo)) return "Unknown";

  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;

  // Morning: 05:00 → 10:59
  if (h >= 5 && h <= 10) {
    return "Good morning";
  }

  // Midday: 11:00 → 13:59
  if (h >= 11 && h <= 13) {
    return "Good midday";
  }

  // Afternoon: 14:00 → 17:29
  if (h >= 14 && (h < 17 || (h == 17 && m < 30))) {
    return "Good afternoon";
  }

  // Evening: 17:30 → 04:59
  return "Good evening";
}
void updateTimeOfDay() {
  if (!timeReady) return;

  String newState = getTimeOfDay();

  if (newState != timeOfDay) {
    timeOfDay = newState;
    publishState("espD/time_of_day", timeOfDay);
  }
}
// ================== WIFI ==================
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
}

// ================== MQTT ==================
void publishState(String topic, String value) {
  client.publish(topic.c_str(), value.c_str());
}

// ================== RELAY ==================
void setRelay(int idx, bool state) {
  relayState[idx] = state;

  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);

  publishState("espD/relay" + String(idx + 1) + "/state", state ? "ON" : "OFF");
}

void toggleRelay(int idx) {
  setRelay(idx, !relayState[idx]);
}

void turnOnRelay(int idx) {
  setRelay(idx, true);
}

void turnOffRelay(int idx) {
  setRelay(idx, false);
}

// ================== SERVO ==================
void setServo(int idx, bool state) {
  servoState[idx] = state;

  int angle = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

  if (idx == 0) servo1.write(angle);
  else servo2.write(angle);

  publishState("espD/servo" + String(idx + 1) + "/state", state ? "ON" : "OFF");
}

void toggleServo(int idx) {
  setServo(idx, !servoState[idx]);
}

void turnOnServo(int idx) {
  setServo(idx, true);
}

void turnOffServo(int idx) {
  setServo(idx, false);
}

// ================== LIGHT ==================
void setLight(bool state) {
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? HIGH : LOW);
  publishState("espD/light/state", state ? "ON" : "OFF");
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  String t = String(topic);

  // relay
  if (t == "espD/relay1/set") {
    if (msg == "ON") turnOnRelay(0);
    else if (msg == "OFF") turnOffRelay(0);
    else if (msg == "TOGGLE") toggleRelay(0);
  }

  if (t == "espD/relay2/set") {
    if (msg == "ON") turnOnRelay(1);
    else if (msg == "OFF") turnOffRelay(1);
    else if (msg == "TOGGLE") toggleRelay(1);
  }

  // servo
  if (t == "espD/servo1/set") {
    if (msg == "ON") turnOnServo(0);
    else if (msg == "OFF") turnOffServo(0);
    else if (msg == "TOGGLE") toggleServo(0);
  }

  if (t == "espD/servo2/set") {
    if (msg == "ON") turnOnServo(1);
    else if (msg == "OFF") turnOffServo(1);
    else if (msg == "TOGGLE") toggleServo(1);
  }
}

// ================== MQTT RECONNECT ==================
void reconnect() {
  while (!client.connected()) {
    if (client.connect("espD")) {
      client.subscribe("espD/relay1/set");
      client.subscribe("espD/relay2/set");
      client.subscribe("espD/servo1/set");
      client.subscribe("espD/servo2/set");
    } else {
      delay(2000);
    }
  }
}

// ================== MOTION ==================

void handleMotion() {
  bool motion = digitalRead(MOTION_PIN);
  bool night = isNightTime();

  if (motion != lastMotionState) {
    lastMotionState = motion;

    if (motion == HIGH && night) {
      setLight(true);   // có người + ban đêm → bật
    } else {
      setLight(false);  // còn lại → tắt
    }

    publishState("espD/motion", motion ? "1" : "0");
  }
}

// ================== TOUCH ==================
void handleTouch() {
  bool currentState = digitalRead(TOUCH_PIN);

  if (lastTouchState == HIGH && currentState == LOW) {
    unsigned long now = millis();

    if (now - lastTouchTime < 500) {
      touchCount++;
    } else {
      touchCount = 1;
    }

    lastTouchTime = now;
  }

  if (touchCount > 0 && millis() - lastTouchTime > 500) {
    if (touchCount == 1) {
      toggleRelay(0); // 1 chạm → relay 1
    }

    // 2,3 chạm chưa xử lý

    touchCount = 0;
  }

  lastTouchState = currentState;
}

// ================== SETUP ==================

void setup() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT_PULLUP);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // chờ lấy thời gian
  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }

  // đánh dấu đã có thời gian
  timeReady = true;
}

// ================== LOOP ==================
void sub_loop_time() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck > 30000) { // mỗi 30s
    updateTimeOfDay();
    lastCheck = millis();
  }

  static unsigned long lastSync = 0;

  if (millis() - lastSync > 3600000) { // mỗi 1 giờ
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    lastSync = millis();
  }
}
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  handleMotion();
  handleTouch();
  sub_loop_time();
}