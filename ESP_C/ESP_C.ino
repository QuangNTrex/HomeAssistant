/*
 * ESP_C — Fixed Version
 * Fixes:
 *  1. reconnect() non-blocking (không dùng while-loop)
 *  2. MQTT buffer tăng lên 512 bytes
 *  3. setKeepAlive(60) để tránh broker ngắt kết nối khi xử lý touch/relay
 *  4. Serial logging đầy đủ để debug
 *  5. Touch debounce ổn định hơn
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";
const int   mqtt_port   = 1883;

// ================== PIN ==================
#define TOUCH   D0   // GPIO16

#define SDA_PIN D1   // LCD
#define SCL_PIN D2   // LCD

#define RELAY1  D5   // GPIO14
#define RELAY2  D6   // GPIO12
#define RELAY3  D7   // GPIO13

#define DHTPIN  D4   // GPIO2
#define DHTTYPE DHT22

// ================== MQTT BUFFER ==================
// FIX: Buffer mặc định 128 bytes không đủ — tăng lên 512
#define MQTT_MAX_PACKET_SIZE 512

// ================== OBJECT ==================
WiFiClient   espClient;
PubSubClient client(espClient);
DHT          dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================== STATE ==================
bool relayState[3] = {false, false, false};

unsigned long lastLCDUpdate = 0;
const long LCD_INTERVAL = 10000;

float lastTemp = 0;
float lastHum  = 0;

// ================== TOUCH ==================
enum TouchPhase { TOUCH_IDLE, TOUCH_COUNTING, TOUCH_HOLDING };
TouchPhase    touchPhase     = TOUCH_IDLE;
bool          lastTouchState = LOW;
int           touchCount     = 0;
unsigned long touchStartTime = 0;
unsigned long lastTouchTime  = 0;

// ================== DHT ==================
unsigned long lastDHTRead    = 0;
const long    DHT_INTERVAL   = 5000;
const long    DOWN_HUMI      = 10;

// ================== RECONNECT ==================
// FIX: Non-blocking reconnect với cooldown
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// ================== LOG HELPER ==================
void log(const String& tag, const String& msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(msg);
}

// ================== WIFI ==================
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

// ================= TIME ======================
void setup_time() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  log("TIME", "Syncing NTP...");
}

// ================== RELAY CONTROL ==================
void publishRelayState(int relay) {
  String topic = "espC/relay" + String(relay + 1) + "/state";
  bool ok = client.publish(topic.c_str(), relayState[relay] ? "ON" : "OFF", true);
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

// ================== MQTT PUBLISH HELPER ==================
bool safePub(const char* topic, const char* payload) {
  if (!client.connected()) {
    log("MQTT", "Publish skipped (disconnected): " + String(topic));
    return false;
  }
  bool ok = client.publish(topic, payload);
  log("MQTT", "Publish " + String(topic) + " = " + String(payload) + (ok ? " OK" : " FAIL (check buffer size)"));
  return ok;
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String t = String(topic);
  log("MQTT", "Received: " + t + " = " + msg);

  if (t == "espC/relay1/set") {
    if      (msg == "ON")     turnOnRelay(0);
    else if (msg == "OFF")    turnOffRelay(0);
    else if (msg == "TOGGLE") toggleRelay(0);
    else log("MQTT", "Unknown command: " + msg);
  }
  else if (t == "espC/relay2/set") {
    if      (msg == "ON")     turnOnRelay(1);
    else if (msg == "OFF")    turnOffRelay(1);
    else if (msg == "TOGGLE") toggleRelay(1);
  }
  else if (t == "espC/relay3/set") {
    if      (msg == "ON")     turnOnRelay(2);
    else if (msg == "OFF")    turnOffRelay(2);
    else if (msg == "TOGGLE") toggleRelay(2);
  }
}

// ================== MQTT RECONNECT (NON-BLOCKING) ==================
// FIX: Không dùng while-loop nữa — chỉ thử 1 lần mỗi RECONNECT_INTERVAL
void reconnect() {
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  log("MQTT", "Attempting reconnect to " + String(mqtt_server) + "...");

  if (client.connect("espC", nullptr, nullptr, "espC/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    client.publish("espC/status", "online", true);
    client.subscribe("espC/relay1/set");
    client.subscribe("espC/relay2/set");
    client.subscribe("espC/relay3/set");
    log("MQTT", "Subscribed to relay topics");
  } else {
    log("MQTT", "Failed, rc=" + String(client.state()) + " — retry in " + String(RECONNECT_INTERVAL/1000) + "s");
    // client.state() codes:
    // -4: timeout, -3: connection lost, -2: connect failed, -1: disconnected
    // 1: bad protocol, 2: bad client ID, 3: server unavailable, 4: bad credentials, 5: unauthorized
  }
}

void handleLCD() {
  if (millis() - lastLCDUpdate < LCD_INTERVAL) return;
  lastLCDUpdate = millis();

  struct tm timeinfo;
  char line1[17];
  char line2[17];

  if (getLocalTime(&timeinfo)) {
    snprintf(line1, sizeof(line1),
             "%02d:%02d %02d/%02d/%02d",
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_mday,
             timeinfo.tm_mon + 1,
             (timeinfo.tm_year + 1900) % 100);
  } else {
    snprintf(line1, sizeof(line1), "No Time");
  }

  snprintf(line2, sizeof(line2),
           "T:%2.0fC H:%2.0f%%",
           lastTemp,
           lastHum);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);

  log("LCD", String(line1) + " | " + String(line2));
}

void handleTouch() {
  bool currentState = digitalRead(TOUCH);
  unsigned long now = millis();

  // Phát hiện chạm (cạnh lên: LOW → HIGH)
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && (now - lastTouchTime < 500)) {
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

    log("TOUCH", "HOLD detected → TOGGLE espD/servo2 (den bep)");
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
    safePub("espD/servo2/set", "TOGGLE");
  }

  // Phát hiện thả tay (cạnh xuống: HIGH → LOW)
  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      log("TOUCH", "Released from HOLD → IDLE");
      touchPhase = TOUCH_IDLE;
    }
  }

  // Timeout multi-tap: 500ms sau lần chạm cuối → thực thi action
  if (touchPhase == TOUCH_COUNTING
      && currentState == LOW
      && (now - lastTouchTime > 500)) {

    log("TOUCH", "Execute tap action, count=" + String(touchCount));

    if      (touchCount == 1) { toggleRelay(0); log("TOUCH", "1 tap → toggle relay1 (local)"); }
    else if (touchCount == 2) { safePub("espD/relay1/set", "TOGGLE"); log("TOUCH", "2 tap → toggle espD/relay1 (quat)"); }
    else if (touchCount == 3) { safePub("espD/servo1/set", "TOGGLE"); log("TOUCH", "3 tap → toggle espD/servo1 (den chinh)"); }
    else if (touchCount == 4) { toggleRelay(1); log("TOUCH", "4 tap → toggle relay2 (man hinh)"); }
    else { log("TOUCH", "No action for count=" + String(touchCount)); }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}

// ================== DHT NON-BLOCKING ==================
void handleDHT() {
  if (millis() - lastDHTRead < DHT_INTERVAL) return;
  lastDHTRead = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    log("DHT", "Read FAILED — check wiring/sensor");
    return;
  }

  h = h - DOWN_HUMI;

  lastTemp = t;
  lastHum  = h;

  log("DHT", "Temp=" + String(t, 1) + "°C  Hum=" + String(h, 1) + "%");
  safePub("espC/temp", String(t, 1).c_str());
  safePub("espC/hum",  String(h, 1).c_str());
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(100);
  log("BOOT", "ESP_C starting...");

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(TOUCH,  INPUT_PULLUP);

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

  // FIX: Tăng buffer MQTT lên 512 bytes
  client.setBufferSize(512);
  // FIX: KeepAlive 60s để broker không ngắt kết nối khi ESP đang xử lý
  client.setKeepAlive(60);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  log("BOOT", "MQTT configured");

  dht.begin();
  log("BOOT", "DHT started");

  log("BOOT", "Setup complete. Free heap: " + String(ESP.getFreeHeap()));
}

// ================== LOOP ==================
void loop() {
  // Log heap mỗi 30s để phát hiện memory leak
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

  handleTouch();
  handleDHT();
  handleLCD();
}
