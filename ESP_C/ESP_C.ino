#define MQTT_MAX_PACKET_SIZE 512

#include <ESP8266WiFi.h>
#include "DHTManager.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <math.h>
#include "PageManager.h"
#include "MqttManager.h"
#include "TouchManager.h"


// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== PIN ==================
#define SDA_PIN D1   // LCD
#define SCL_PIN D2   // LCD

#define RELAY1  D5   // GPIO14
#define RELAY2  D6   // GPIO12
#define RELAY3  D7   // GPIO13

#define TRIG_PIN D3  // GPIO0 - HC-SR04 Trigger
#define ECHO_PIN D8  // GPIO15 - HC-SR04 Echo

#define TOUCH_PIN D0
#define TOUCH_PIN_2 A0

// ================= ESPD_STATE ==================
String ESPD_RELAY1_SET = "espD/relay1/set";
String ESPD_RELAY2_SET = "espD/relay2/set";
String ESPD_SERVO1_SET = "espD/servo1/set";
String ESPD_SERVO2_SET = "espD/servo2/set";
String ESPD_FAN_SET = "espD/fan/set";

String ESPD_RELAY1_STATE = "espD/relay1/state";
String ESPD_RELAY2_STATE = "espD/relay2/state";
String ESPD_SERVO1_STATE = "espD/servo1/state";
String ESPD_SERVO2_STATE = "espD/servo2/state";
String ESPD_FAN_STATE = "espD/fan/state";
String ESP_TEMP = "espD/temp";
String ESP_HUM = "espD/hum";

// biến lưu trạng thái thiết bị bên ESP_D, cập nhật khi nhận MQTT
bool espDRelayStates[3] = {false, false, false};
bool espDServoStates[3] = {false, false, false};
bool espDFanState = false;
float espDTemp = 0;
float espDHum = 0;

// kiểm tra xem có cập nhật mới từ ESP_D không (MQTT callback sẽ set flag này)
long espDRelayUpdatedAt[3] = {0, 0, 0};
long espDServoUpdatedAt[3] = {0, 0, 0};
long espDFanUpdatedAt = 0;

// ================== OBJECT ==================
WiFiClient   espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
PageManager pageManager(lcd, client);
// ================== STATE ==================
bool relayState[3] = {false, false, false};

// ================ LIGHT ====================
bool lcdBacklight = true;

unsigned long lastBacklightOn = 0;
const unsigned long BACKLIGHT_AUTO_OFF_TIMEOUT = 600000; // 10 minutes
const float BACKLIGHT_DISTANCE_THRESHOLD = 100.0;       // cm
unsigned long BACKLIGHT_SET_BY_USER_AT = 0;
const unsigned long BACKLIGHT_TOUCH_COOLDOWN = 10 * 60 * 1000; // 10 phút sau khi touch thì ignore ultrasonic

unsigned long lastLCDUpdate = 0;
const long LCD_INTERVAL = 5000;

float lastTemp = 0;
float lastHum  = 0;

// ================ PAGE ====================
int currentPage = PAGE_CLOCK;

unsigned long lastPageUpdate = 0;
const unsigned long PAGE_INTERVAL = 8000; // 8s clock ↔ greeting

// Page event

// ================== HC-SR04 ULTRASONIC ==================
unsigned long lastDistanceRead = 0;
const long    DISTANCE_INTERVAL = 2000;  // 2 seconds
float         lastDistance = -1;         // lưu giá trị cũ để so sánh
unsigned long lastCloseDetectionTime = 0;  // khi đầu tiên phát hiện khoảng cách < 100cm
const unsigned long CLOSE_DETECTION_TIMEOUT = 5000;  // phải detect liên tục trong 5s mới bật
// ================== RECONNECT ==================
// FIX: Non-blocking reconnect với cooldown

const unsigned long RECONNECT_INTERVAL = 5000;

unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000;

unsigned long wifiLostSince = 0;
const unsigned long WIFI_DEAD_TIMEOUT = 60000; // 60s

unsigned long lastStatusPub = 0;
const unsigned long STATUS_INTERVAL = 30000; // 30s

// ================== LOG HELPER ==================
void log(const String& tag, const String& msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(msg);

  // Lưu log để hiển thị trên pageLog
  pageManager.setLog(tag, msg);
}

// ================== WIFI ==================
void setup_wifi() {
  WiFi.setAutoReconnect(true);
  log("WIFI", "Connecting to: " + String(ssid));
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // 🔥 tránh sleep gây mất kết nối
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false); // 🔥 tránh flash corruption

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

// ================= TIME ======================
void setup_time() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  log("TIME", "Syncing NTP...");
}

// ================= NIGHT =====================

bool isNight() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;

  int hour = timeinfo.tm_hour;

  // 22h → 23h hoặc 0h → 5h
  return (hour >= 22 || hour < 5);
}

// ================
void setBacklight(bool state) {
  if (lcdBacklight == state) return;

  lcdBacklight = state;

  if (state) {
    lcd.backlight();
    lastBacklightOn = millis(); // reset timer
  } else {
    lcd.noBacklight();
  }

  // 🔥 sync Home Assistant
  safePub("espC/lcd/backlight/state", state ? "ON" : "OFF", true);

  log("LCD", String("Backlight → ") + (state ? "ON" : "OFF"));
}

void toggleBacklight() {
  lcdBacklight = !lcdBacklight;

  if (lcdBacklight) {
    lcd.backlight();
    lastBacklightOn = millis(); // reset timer
  } else {
    lcd.noBacklight();
  }

  // 🔥 sync Home Assistant
  safePub("espC/lcd/backlight/state", lcdBacklight ? "ON" : "OFF", true);

  log("LCD", String("Backlight → ") + (lcdBacklight ? "ON" : "OFF"));
}

// ================== RELAY CONTROL ==================
void publishRelayState(int relay) {
  String topic = "espC/relay" + String(relay + 1) + "/state";
  bool ok = safePub(topic.c_str(), relayState[relay] ? "ON" : "OFF", true);
  log("RELAY", "Publish " + topic + " = " + (relayState[relay] ? "ON" : "OFF") + (ok ? " OK" : " FAIL"));
}

void setRelay(int relay, bool state) {
  if (relayState[relay] == state) {
    log("RELAY", "relay" + String(relay+1) + " already " + (state ? "ON" : "OFF") + ", skip");
    return;
  }
  relayState[relay] = state;
  int pin = (relay == 0) ? RELAY1 : (relay == 1) ? RELAY2 : RELAY3;
  digitalWrite(pin, state ? HIGH : LOW);
  log("RELAY", "relay" + String(relay+1) + " → " + (state ? "ON" : "OFF"));
  publishRelayState(relay);
}

void toggleRelay(int relay)  { setRelay(relay, !relayState[relay]); }
void turnOnRelay(int relay)  { setRelay(relay, true);  }
void turnOffRelay(int relay) { setRelay(relay, false); }

void showEvent(const String& l1, const String& l2) {
  pageManager.showEvent(l1, l2, currentPage, lastLCDUpdate);
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t = String(topic);
  log("MQTT", "Received: " + t + " = " + msg);

  if (t == "espC/relay1/set") {
    if      (msg == "ON")     { turnOnRelay(0);  showEvent("Relay 1", "Turned ON");  }
    else if (msg == "OFF")    { turnOffRelay(0); showEvent("Relay 1", "Turned OFF"); }
    else if (msg == "TOGGLE") { toggleRelay(0);  showEvent("Relay 1", relayState[0] ? "-> ON" : "-> OFF"); }
    else log("MQTT", "Unknown command: " + msg);
  }
  else if (t == "espC/relay2/set") {
    if      (msg == "ON")     { turnOnRelay(1);  showEvent("Relay 2", "Turned ON");  }
    else if (msg == "OFF")    { turnOffRelay(1); showEvent("Relay 2", "Turned OFF"); }
    else if (msg == "TOGGLE") { toggleRelay(1);  showEvent("Relay 2", relayState[1] ? "-> ON" : "-> OFF"); }
  }
  else if (t == "espC/relay3/set") {
    if      (msg == "ON")     { turnOnRelay(2);  showEvent("Relay 3", "Turned ON");  }
    else if (msg == "OFF")    { turnOffRelay(2); showEvent("Relay 3", "Turned OFF"); }
    else if (msg == "TOGGLE") { toggleRelay(2);  showEvent("Relay 3", relayState[2] ? "-> ON" : "-> OFF"); }
  }
  else if (t == "espC/lcd/backlight/set") {
    if      (msg == "ON")  { setBacklight(true);  showEvent("Backlight", "Turned ON");  }
    else if (msg == "OFF") { setBacklight(false); showEvent("Backlight", "Turned OFF"); BACKLIGHT_SET_BY_USER_AT = millis(); } // khi tắt bằng MQTT thì coi như user set, bật lại sẽ phải chờ cooldown
  }
  else if (t == "espD/relay1/state") {
    espDRelayStates[0] = (msg == "ON");
    espDRelayUpdatedAt[0] = millis();
    log("MQTT", "Updated ESP_D Relay 1 state: " + String(msg));
    showEvent("ESP_D Relay 1", String("State: ") + (espDRelayStates[0] ? "ON" : "OFF"));
  }
  else if (t == "espD/relay2/state") {
    espDRelayStates[1] = (msg == "ON");
    espDRelayUpdatedAt[1] = millis();
    log("MQTT", "Updated ESP_D Relay 2 state: " + String(msg));
    showEvent("ESP_D Relay 2", String("State: ") + (espDRelayStates[1] ? "ON" : "OFF"));
  }
  else if (t == "espD/servo1/state") {
    espDServoStates[0] = (msg == "ON");
    espDServoUpdatedAt[0] = millis();
    log("MQTT", "Updated ESP_D Servo 1 state: " + String(msg));
    showEvent("ESP_D Servo 1", String("State: ") + (espDServoStates[0] ? "ON" : "OFF"));
  }
  else if (t == "espD/servo2/state") {
    espDServoStates[1] = (msg == "ON");
    espDServoUpdatedAt[1] = millis();
    log("MQTT", "Updated ESP_D Servo 2 state: " + String(msg));
    showEvent("ESP_D Servo 2", String("State: ") + (espDServoStates[1] ? "ON" : "OFF"));
  }
  else if (t == "espD/fan/state") {
    espDFanState = (msg == "ON");
    espDFanUpdatedAt = millis();
    log("MQTT", "Updated ESP_D Fan state: " + String(msg));
    showEvent("ESP_D Fan", String("State: ") + (espDFanState ? "ON" : "OFF"));
  }
  else if (t == "espD/temp") {
    espDTemp = msg.toFloat();
  }
  else if (t == "espD/hum") {
    espDHum = msg.toFloat();
  }
}

void handleLCD() {
  pageManager.handleLCD(currentPage, lastLCDUpdate, LCD_INTERVAL, lastTemp, lastHum);
}

// ================== HC-SR04 ULTRASONIC ==================
float measureDistance() {
  // Gửi xung trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Đợi nhận tín hiệu echo
  unsigned long startTime = micros();
  unsigned long timeout = startTime + 23000;  // Timeout ~4m (23ms)
  
  while (digitalRead(ECHO_PIN) == LOW && micros() < timeout) {
    // Chờ ECHO PIN lên HIGH
  }
  unsigned long pulseStart = micros();

  timeout = pulseStart + 23000;
  while (digitalRead(ECHO_PIN) == HIGH && micros() < timeout) {
    // Chờ ECHO PIN xuống LOW
  }
  unsigned long pulseEnd = micros();

  // Tính khoảng cách: vận tốc âm thanh = 343 m/s = 0.0343 cm/µs
  // Thời gian đi và về nên chia cho 2
  unsigned long pulseDuration = pulseEnd - pulseStart;
  float distance = (pulseDuration * 0.0343) / 2.0;

  // Hạn chế khoảng cách hợp lý (cm): từ 2cm đến 400cm
  if (distance < 2 || distance > 400) {
    return -1;  // Giá trị không hợp lệ
  }

  return distance;
}

void handleUltrasonic() {
  if (millis() - lastDistanceRead < DISTANCE_INTERVAL) return;
  lastDistanceRead = millis();

  float distance = measureDistance();

  // Bỏ qua nếu đo không hợp lệ
  if (distance < 0) {
    log("ULTRASONIC", "Measurement out of range");
    lastCloseDetectionTime = 0;  // reset timer khi đo không hợp lệ
    return;
  }

  // Debounce: phát hiện khoảng cách < ngưỡng phải liên tục trong 2 giây
  if (distance < BACKLIGHT_DISTANCE_THRESHOLD) {
    // Nếu này là lần đầu detect gần, ghi nhận thời gian
    if (lastCloseDetectionTime == 0) {
      lastCloseDetectionTime = millis();
      log("ULTRASONIC", "Close detection started (" + String(distance, 1) + "cm) - waiting 2s...");
    } else {
      // Kiểm tra xem đã detect liên tục được 2 giây chưa
      // phần bật đèn tự động sử dụng cảm biến siêu âm
      unsigned long detectionDuration = millis() - lastCloseDetectionTime;
      if (detectionDuration >= CLOSE_DETECTION_TIMEOUT && !lcdBacklight && millis() - BACKLIGHT_SET_BY_USER_AT > BACKLIGHT_TOUCH_COOLDOWN) { // chỉ bật nếu chưa bật và đã detect đủ lâu và đã đủ lâu sau touch
        setBacklight(true);
        log("LCD", "Ultrasonic detected presence (" + String(detectionDuration) + "ms) → backlight ON");
      }
    }
  } else {
    // Nếu khoảng cách > ngưỡng, reset timer
    if (lastCloseDetectionTime > 0) {
      log("ULTRASONIC", "Close detection cancelled (distance=" + String(distance, 1) + "cm)");
      lastCloseDetectionTime = 0;
    }
  }

  // Chỉ publish nếu giá trị thay đổi (so sánh với sai số 0.5cm)
  if (lastDistance < 0 || fabs(distance - lastDistance) > 0.5) {
    lastDistance = distance;
    log("ULTRASONIC", "Distance=" + String(distance, 1) + "cm");
    safePub("espC/distance", String(distance, 1).c_str());
  }
}

void handleWiFi() {
  wl_status_t status = WiFi.status();

  // ✅ Nếu đã có mạng → reset timer
  if (status == WL_CONNECTED) {
    wifiLostSince = 0;
    return;
  }

  unsigned long now = millis();

  // ✅ Bắt đầu tính thời gian mất mạng
  if (wifiLostSince == 0) {
    wifiLostSince = now;
    log("WIFI", "Lost connection → start timer");
  }

  // 🔥 Nếu mất quá lâu → reboot
  if (now - wifiLostSince > WIFI_DEAD_TIMEOUT) {
    log("WIFI", "Dead >60s → REBOOT ESP");
    ESP.restart();
  }

  // ⏱ retry có interval
  if (now - lastWiFiAttempt < WIFI_RECONNECT_INTERVAL) return;
  lastWiFiAttempt = now;

  log("WIFI", "Reconnect attempt... status=" + String(status));

  // 🔥 RESET MẠNH WiFi stack (cực quan trọng)
  WiFi.disconnect(true);   // xóa config cũ
  delay(100);

  WiFi.mode(WIFI_OFF);
  delay(200);

  WiFi.mode(WIFI_STA);
  delay(200);

  WiFi.begin(ssid, password);
}

void handleBacklightAutoOff() {
  if (lcdBacklight && millis() - lastBacklightOn >= BACKLIGHT_AUTO_OFF_TIMEOUT) {
    if (lastDistance > BACKLIGHT_DISTANCE_THRESHOLD) {
      setBacklight(false);
      log("LCD", "Backlight auto-off after timeout");
    }
    else {
      lastBacklightOn = millis(); // reset timer nếu vẫn có người
    }
  }
}
// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(100);
  log("BOOT", "ESP_C starting...");

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  touchBegin();

  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  log("BOOT", "Pins initialized");

  setup_wifi();

  Wire.begin(D1, D2);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Booting...");
  delay(5000);
  setup_time();

  mqttBegin(callback);
  log("BOOT", "MQTT configured");

  dhtBegin();
  log("BOOT", "DHT started");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  log("BOOT", "HC-SR04 pins configured");

  log("BOOT", "Setup complete. Free heap: " + String(ESP.getFreeHeap()));
}

// ================== LOOP ==================
void loop() {
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    lastHeapLog = millis();
    log("HEAP", "Free heap: " + String(ESP.getFreeHeap()) +
        " | WiFi RSSI: " + String(WiFi.RSSI()) +
        " dBm | MQTT: " + (mqttIsConnected() ? "OK" : "DISCONNECTED"));
  }

  handleWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttIsConnected()) mqttReconnect();
    mqttLoop();
    // giữ mqtt luôn sống
    if (mqttIsConnected() && millis() - lastStatusPub > STATUS_INTERVAL) {
      lastStatusPub = millis();
      safePub("espC/status", "online", true);
      log("MQTT", "Heartbeat published");
    }
  }

  handleTouch(currentPage, lastPageUpdate);
  handleDHT(lastTemp, lastHum);
  handleUltrasonic();
  
  handleBacklightAutoOff();
  
  handleLCD();
  handleSecondaryTouch();
}
