#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ================== WIFI ==================
const char* ssid = "Test";
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
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// ================== STATE ==================
bool relayState[3] = {false, false, false};

// touch
unsigned long lastTouchTime = 0;
bool lastTouchState = HIGH;
int touchCount = 0;
unsigned long touchStartTime = 0;
bool holdTriggered = false;

// DHT
unsigned long lastDHTRead = 0;
const long DHT_INTERVAL = 5000;

// ================== WIFI ==================
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
}

// ================== MQTT ==================
void publishRelayState(int relay) {
  String topic = "espC/relay" + String(relay + 1) + "/state";
  client.publish(topic.c_str(), relayState[relay] ? "ON" : "OFF");
}

// ================== RELAY CONTROL ==================
void setRelay(int relay, bool state) {
  relayState[relay] = state;
  digitalWrite(relay == 0 ? RELAY1 : relay == 1 ? RELAY2 : RELAY3, state ? HIGH : LOW);
  publishRelayState(relay);
}

void toggleRelay(int relay) {
  setRelay(relay, !relayState[relay]);
}

void turnOnRelay(int relay) {
  setRelay(relay, true);
}

void turnOffRelay(int relay) {
  setRelay(relay, false);
}

// ================== MQTT CALLBACK ==================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  String t = String(topic);

  if (t == "espC/relay1/set") {
    if (msg == "ON") turnOnRelay(0);
    else if (msg == "OFF") turnOffRelay(0);
    else if (msg == "TOGGLE") toggleRelay(0);
  }

  if (t == "espC/relay2/set") {
    if (msg == "ON") turnOnRelay(1);
    else if (msg == "OFF") turnOffRelay(1);
    else if (msg == "TOGGLE") toggleRelay(1);
  }

  if (t == "espC/relay3/set") {
    if (msg == "ON") turnOnRelay(2);
    else if (msg == "OFF") turnOffRelay(2);
    else if (msg == "TOGGLE") toggleRelay(2);
  }
}

// ================== MQTT RECONNECT ==================
void reconnect() {
  while (!client.connected()) {
    if (client.connect("espC", nullptr, nullptr, "espC/status", 0, true, "offline")) {
      client.publish("espC/status", "online", true);
      client.subscribe("espC/relay1/set");
      client.subscribe("espC/relay2/set");
      client.subscribe("espC/relay3/set");
    } else {
      delay(2000);
    }
  }
}

// ================== TOUCH HANDLE ==================
void handleTouch() {
  bool currentState = digitalRead(TOUCH);
  unsigned long now = millis();

  // ===== Phát hiện chạm (cạnh lên: LOW → HIGH) =====
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (now - lastTouchTime < 500) {
      touchCount++;
    } else {
      touchCount = 1;
    }

    lastTouchTime = now;
  }

  // ===== Hold: giữ > 2s, chỉ khi single tap =====
  if (currentState == HIGH
      && touchCount == 1
      && !holdTriggered
      && now - touchStartTime > 2000) {

    holdTriggered = true;
    touchCount = 0;
    client.publish("espD/servo2/set", "TOGGLE"); // den bep
  }

  // ===== Reset hold khi thả tay =====
  if (lastTouchState == HIGH && currentState == LOW) {
    holdTriggered = false;
  }

  // ===== Xử lý sau khi hết chuỗi chạm (timeout 500ms) =====
  if (touchCount > 0 && !holdTriggered && currentState == LOW
      && now - lastTouchTime > 500) {

    if      (touchCount == 1) toggleRelay(0);
    else if (touchCount == 2) client.publish("espD/relay1/set", "TOGGLE"); // bat quat
    else if (touchCount == 3) client.publish("espD/servo1/set", "TOGGLE"); // bat den chinh
    else if (touchCount == 4) toggleRelay(1); // bat tat man hinh

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
      client.publish("espC/temp", String(t).c_str());
      client.publish("espC/hum", String(h).c_str());
    }
  }
}

// ================== SETUP ==================
void setup() {
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(TOUCH, INPUT_PULLUP);

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
  client.loop();

  handleTouch();
  handleDHT();
}