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

// ================ LIGHT ====================
bool lcdBacklight = true;

unsigned long lastBacklightOn = 0;
const unsigned long BACKLIGHT_TIMEOUT = 30000; // 30s

unsigned long lastLCDUpdate = 0;
const long LCD_INTERVAL = 5000;

float lastTemp = 0;
float lastHum  = 0;

// ================ PAGE ====================
int lastPage = -1; // page đang hiển thị trước đó

enum Page { PAGE_CLOCK = 0, PAGE_GREETING = 1, PAGE_SYSTEM = 2, PAGE_EVENT = 3 };
int currentPage = PAGE_CLOCK;

unsigned long lastPageUpdate = 0;
const unsigned long PAGE_INTERVAL = 8000; // 8s clock ↔ greeting

// Page event
String eventLine1 = "";
String eventLine2 = "";
unsigned long eventShownAt   = 0;  // thời điểm event/system được kích hoạt
const unsigned long RETURN_TO_CLOCK = 8000; // 5s rồi về clock

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
  // client.publish("espC/lcd/backlight/state",
  //                state ? "ON" : "OFF",
  //                true);

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
  // client.publish("espC/lcd/backlight/state",
  //                lcdBacklight ? "ON" : "OFF",
  //                true);

  log("LCD", String("Backlight → ") + (lcdBacklight ? "ON" : "OFF"));
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

///////////////////////////////

void showEvent(const String& l1, const String& l2) {
  eventLine1 = l1.substring(0, 16); // giới hạn 16 ký tự LCD
  eventLine2 = l2.substring(0, 16);
  currentPage  = PAGE_EVENT;
  eventShownAt = millis();
  lastLCDUpdate = 0; // force render ngay lập tức
  lcd.clear();
  log("EVENT", l1 + " | " + l2);
}

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

bool safePub(const char* topic, const char* payload, bool retain) {
  if (!client.connected()) {
    log("MQTT", "Publish skipped (disconnected): " + String(topic));
    return false;
  }
  bool ok = client.publish(topic, payload, retain);
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
    else if (msg == "OFF") { setBacklight(false); showEvent("Backlight", "Turned OFF"); }
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
    client.subscribe("espC/lcd/backlight/set");

    client.publish("espC/lcd/backlight/state",
               lcdBacklight ? "ON" : "OFF",
               true);
    log("MQTT", "Subscribed to relay topics");
  } else {
    log("MQTT", "Failed, rc=" + String(client.state()) + " — retry in " + String(RECONNECT_INTERVAL/1000) + "s");
    // client.state() codes:
    // -4: timeout, -3: connection lost, -2: connect failed, -1: disconnected
    // 1: bad protocol, 2: bad client ID, 3: server unavailable, 4: bad credentials, 5: unauthorized
  }
}

void pageClock() {
  struct tm timeinfo;
  char line1[17];
  char line2[17];

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

  snprintf(line2, sizeof(line2),
           "T:%2.1fC H:%2.1f%%",
           lastTemp, lastHum);

  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2);
}

void pageSystem() {
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1),
           "WiFi:%ddBm", WiFi.RSSI());

  snprintf(line2, sizeof(line2),
           "MQTT:%s",
           client.connected() ? "OK" : "FAIL");

  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2);
}

// ================== PAGE EVENT ==================
void pageEvent() {
  lcd.setCursor(0, 0); lcd.print(eventLine1);
  lcd.setCursor(0, 1); lcd.print(eventLine2);
}

void pageGreeting() {
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

void handleLCD() {
  unsigned long now = millis();
  // static int lastPage = -1;

  // --- Logic tự động chuyển page ---

  // EVENT & SYSTEM: sau 5s về CLOCK
  if ((currentPage == PAGE_EVENT || currentPage == PAGE_SYSTEM)
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
    case PAGE_CLOCK:    pageClock();    break;
    case PAGE_GREETING: pageGreeting(); break;
    case PAGE_SYSTEM:   pageSystem();   break;
    case PAGE_EVENT:    pageEvent();    break;
  }

  lastPage = currentPage;
}

void handleHold() {
  toggleBacklight();
  log("TOUCH", "HOLD detected → TOGGLE backlight");
}

void singleTouch() {

}

void doubleTouch() {

}

void tripleTouch() {

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

    handleHold();
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
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

    // if      (touchCount == 1) { toggleRelay(0); log("TOUCH", "1 tap → toggle relay1 (local)"); setBacklight(true); }
    if (touchCount == 1) {
      // Cycle qua 3 pages: CLOCK(0) → GREETING(1) → SYSTEM(2) → CLOCK(0)
      // Từ EVENT hoặc bất kỳ page nào cũng bước tiếp theo chu kỳ
      int next = (currentPage == PAGE_EVENT)
                ? PAGE_CLOCK
                : (currentPage + 1) % 3; // chỉ cycle trong 0,1,2
      currentPage    = next;
      lastPageUpdate = millis();
      eventShownAt   = (next == PAGE_SYSTEM) ? millis() : 0; // SYSTEM dùng timer về clock
      lcd.clear();
      setBacklight(true);
      log("TOUCH", "1 tap → page " + String(currentPage));
    }
    else if (touchCount == 2) { toggleRelay(0); log("TOUCH", "2 tap → toggle relay1 (local)"); showEvent("Tap Relay 1", relayState[0] ? "Turned ON" : "Turned OFF");} // bật đèn
    else if (touchCount == 3) { toggleRelay(1); log("TOUCH", "3 tap → toggle relay2 (man hinh)");  showEvent("Tap Relay 2", relayState[1] ? "Turned ON" : "Turned OFF"); } // bật màn hình
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
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    lastHeapLog = millis();
    log("HEAP", "Free heap: " + String(ESP.getFreeHeap()) +
        " | WiFi RSSI: " + String(WiFi.RSSI()) +
        " dBm | MQTT: " + (client.connected() ? "OK" : "DISCONNECTED"));
  }

  handleWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnect();
    client.loop();
    // giữ mqtt luôn sống
    if (client.connected() && millis() - lastStatusPub > STATUS_INTERVAL) {
      lastStatusPub = millis();
      client.publish("espC/status", "online", true);
      log("MQTT", "Heartbeat published");
    }
  }

  handleTouch();
  handleDHT();
  handleLCD();
}
