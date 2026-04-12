#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";

// ================== PIN ==================
#define RELAY1 D1
#define RELAY2 D2
#define RELAY3 D5
#define TOUCH  D7
#define DHTPIN D6
#define DHTTYPE DHT22

// ================== OBJECT ==================
WiFiClient   espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// ================== STATE ==================
bool relayState[3] = {false, false, false};

// touch
// FIX 2: INPUT_PULLUP → idle = HIGH, chạm = LOW
//         → cạnh xuống HIGH→LOW = chạm, cạnh lên LOW→HIGH = thả
bool          lastTouchState = HIGH;   // idle HIGH khi dùng INPUT_PULLUP
int           touchCount     = 0;
unsigned long touchStartTime = 0;
unsigned long lastTouchTime  = 0;
bool          holdTriggered  = false;

// DHT
unsigned long lastDHTRead = 0;
const long    DHT_INTERVAL = 5000;

// FIX 1: Cooldown reconnect, không dùng while+delay blocking
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// ================== WIFI ==================
void setup_wifi() {
  Serial.printf("Connecting to Wi-Fi %s...\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
    if (millis() - start > 10000) break;
  }
  Serial.println();
  Serial.printf("Wi-Fi %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "failed");
}

// ================== RELAY CONTROL ==================
void publishRelayState(int relay) {
  String topic = "espC/relay" + String(relay + 1) + "/state";
  client.publish(topic.c_str(), relayState[relay] ? "ON" : "OFF");
}

void setRelay(int relay, bool state) {
  if (relayState[relay] == state) return; // guard: tránh spam
  relayState[relay] = state;
  int pin = (relay == 0) ? RELAY1 : (relay == 1) ? RELAY2 : RELAY3;
  digitalWrite(pin, state ? HIGH : LOW);
  publishRelayState(relay);
}

void toggleRelay(int relay) { setRelay(relay, !relayState[relay]); }
void turnOnRelay(int relay)  { setRelay(relay, true);               }
void turnOffRelay(int relay) { setRelay(relay, false);              }

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  char msg[16] = {0};
  unsigned int copyLen = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, copyLen);

  if (strcmp(topic, "espC/relay1/set") == 0) {
    if      (strcmp(msg, "ON")     == 0) turnOnRelay(0);
    else if (strcmp(msg, "OFF")    == 0) turnOffRelay(0);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(0);
    return;
  }
  if (strcmp(topic, "espC/relay2/set") == 0) {
    if      (strcmp(msg, "ON")     == 0) turnOnRelay(1);
    else if (strcmp(msg, "OFF")    == 0) turnOffRelay(1);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(1);
    return;
  }
  if (strcmp(topic, "espC/relay3/set") == 0) {
    if      (strcmp(msg, "ON")     == 0) turnOnRelay(2);
    else if (strcmp(msg, "OFF")    == 0) turnOffRelay(2);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(2);
  }
}

// ================== MQTT RECONNECT ==================
// FIX 1: Non-blocking, có cooldown 5s
void reconnect() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  Serial.println("MQTT reconnect...");
  if (client.connect("espC", nullptr, nullptr, "espC/status", 0, true, "offline")) {
    Serial.println("MQTT connected");
    client.publish("espC/status", "online", true);
    client.subscribe("espC/relay1/set");
    client.subscribe("espC/relay2/set");
    client.subscribe("espC/relay3/set");
  } else {
    Serial.printf("MQTT connect failed: %d\n", client.state());
  }
}

// ================== TOUCH HANDLE ==================
// FIX 2: INPUT_PULLUP → chạm = LOW, thả = HIGH
//         Cạnh xuống (HIGH→LOW) = bắt đầu chạm
//         Cạnh lên   (LOW→HIGH) = thả tay
void handleTouch() {
  bool currentState = digitalRead(TOUCH);
  unsigned long now = millis();

  // ===== Phát hiện chạm: cạnh xuống HIGH→LOW =====
  if (lastTouchState == HIGH && currentState == LOW) {
    touchStartTime = now;

    if (now - lastTouchTime < 500) {
      touchCount++;
    } else {
      touchCount = 1;
    }

    lastTouchTime = now;
  }

  // ===== Hold: giữ > 2s trong khi đang chạm (LOW), chỉ khi single tap =====
  if (currentState == LOW
      && touchCount == 1
      && !holdTriggered
      && now - touchStartTime > 2000) {

    holdTriggered = true;
    touchCount    = 0;
    client.publish("espD/servo2/set", "TOGGLE"); // đèn bếp
  }

  // ===== Reset hold khi thả tay: cạnh lên LOW→HIGH =====
  if (lastTouchState == LOW && currentState == HIGH) {
    holdTriggered = false;
  }

  // ===== Timeout 500ms sau lần chạm cuối → thực thi =====
  if (touchCount > 0
      && !holdTriggered
      && currentState == HIGH        // tay đã thả (HIGH = idle)
      && now - lastTouchTime > 500) {

    switch (touchCount) {
      case 1:
        toggleRelay(0);
        break;
      case 2:
        if (client.connected()) {
          Serial.println("ESP_C: publish espD/relay1/set TOGGLE");
          client.publish("espD/relay1/set", "TOGGLE");
        } else {
          Serial.println("ESP_C: MQTT disconnected, cannot send espD/relay1/set");
        }
        break;
      case 3:
        if (client.connected()) {
          Serial.println("ESP_C: publish espD/servo1/set TOGGLE");
          client.publish("espD/servo1/set", "TOGGLE");
        } else {
          Serial.println("ESP_C: MQTT disconnected, cannot send espD/servo1/set");
        }
        break;
      case 4:
        toggleRelay(1); // bật tắt màn hình
        break;
    }

    touchCount = 0;
  }

  lastTouchState = currentState;
}

// ================== DHT NON-BLOCKING ==================
void handleDHT() {
  if (millis() - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      client.publish("espC/temp", String(t, 1).c_str());
      client.publish("espC/hum",  String(h, 1).c_str());
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  Serial.println("ESP_C starting...");

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(TOUCH,  INPUT_PULLUP); // idle = HIGH, chạm kéo xuống LOW

  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();
}

// ================== LOOP ==================
void loop() {
  if (!client.connected()) reconnect();
  client.loop(); // FIX: luôn gọi client.loop() để không mất packet

  handleTouch();
  handleDHT();
}
