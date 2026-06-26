#include "DeviceManager.h"
#include "MqttManager.h"
#include "TouchManager.h"
#include <ESP8266WiFi.h>

// ================== WIFI CONFIG ==================
const char *ssid = "Test";
const char *password = "24082002";

// ================== WIFI WATCHDOG VARS ==================
unsigned long wifiLostSince = 0;
const unsigned long WIFI_DEAD_TIMEOUT = 60000; // 60s loss = restart
unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000; // retry wifi every 5s

// ================== CLIENT OBJECTS ==================
WiFiClient espClient;
PubSubClient client(espClient);

// ================== LOGGING SYSTEM ==================
void log(const String &tag, const String &msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(tag);
  Serial.print("] ");
  Serial.println(msg);
}

// ================== WIFI CONNECT & MONITOR ==================
void setup_wifi() {
  WiFi.setAutoReconnect(true);
  log("WIFI", "Connecting to SSID: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable sleep to avoid disconnects
  WiFi.persistent(false);             // Protect flash memory
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 15000) {
      log("WIFI", "Timeout! Continuing, watchdog will handle connection...");
      return;
    }
  }
  log("WIFI", "Connected! IP: " + WiFi.localIP().toString());
}

void handleWiFi() {
  wl_status_t status = WiFi.status();

  // Connection is active, reset loss timer
  if (status == WL_CONNECTED) {
    wifiLostSince = 0;
    return;
  }

  unsigned long now = millis();

  // Connection lost, record timestamp
  if (wifiLostSince == 0) {
    wifiLostSince = now;
    log("WIFI", "Connection lost! Watchdog timer started.");
  }

  // Reboot if connection is lost for more than 60s
  if (now - wifiLostSince > WIFI_DEAD_TIMEOUT) {
    log("WIFI", "Connection lost for > 60s. Rebooting ESP!");
    ESP.restart();
  }

  // Non-blocking reconnection logic
  if (now - lastWiFiAttempt < WIFI_RECONNECT_INTERVAL)
    return;
  lastWiFiAttempt = now;

  log("WIFI", "Attempting connection retry. Status: " + String(status));

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(ssid, password);
}

// ================== MQTT ROUTER CALLBACK ==================
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  String t = String(topic);
  log("MQTT", "Received: " + t + " = " + msg);

  if (t == "espE/buzzer/set") {
    if (msg == "ON")
      setBuzzer(true);
    else if (msg == "OFF")
      setBuzzer(false);
    else if (msg == "TOGGLE")
      toggleBuzzer();
  } else if (t == "espE/led/set") {
    if (msg == "ON")
      setLED(true);
    else if (msg == "OFF")
      setLED(false);
    else if (msg == "TOGGLE")
      toggleLED();
  } else if (t == "espE/servo1/set") {
    if (msg == "ON")
      setServoLight(true);
    else if (msg == "OFF")
      setServoLight(false);
    else if (msg == "TOGGLE")
      toggleServoLight();
  } else if (t == "espE/motor1/set") {
    if (msg == "ON")
      setMotorState(1, true);
    else if (msg == "OFF")
      setMotorState(1, false);
    else if (msg == "TOGGLE")
      toggleMotorState(1);
    else {
      int speed = msg.toInt();
      setMotorSpeed(1, speed);
    }
  } else if (t == "espE/motor2/set") {
    if (msg == "ON")
      setMotorState(2, true);
    else if (msg == "OFF")
      setMotorState(2, false);
    else if (msg == "TOGGLE")
      toggleMotorState(2);
    else {
      int speed = msg.toInt();
      setMotorSpeed(2, speed);
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(100);
  log("BOOT", "ESP_E starting...");

  setupDevices();
  touchBegin();
  setup_wifi();

  mqttBegin(mqttCallback);
  log("BOOT", "Setup complete. Free heap: " + String(ESP.getFreeHeap()));
}

// ================== LOOP ==================
void loop() {
  // Free heap and wifi signal strength diagnostics every 30s
  static unsigned long lastDiagnostics = 0;
  if (millis() - lastDiagnostics > 30000) {
    lastDiagnostics = millis();
    log("HEAP",
        "Free heap: " + String(ESP.getFreeHeap()) +
            " | WiFi RSSI: " + String(WiFi.RSSI()) + " dBm" +
            " | MQTT Status: " + (mqttIsConnected() ? "OK" : "DISCONNECTED"));
  }

  // Handle Wi-Fi watchdog
  handleWiFi();

  // If Wi-Fi is connected, run MQTT loop
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttIsConnected()) {
      mqttReconnect();
    }
    mqttLoop();

    // Heartbeat status publisher every 30 seconds
    static unsigned long lastHeartbeat = 0;
    if (mqttIsConnected() && millis() - lastHeartbeat > 30000) {
      lastHeartbeat = millis();
      safePub("espE/heartbeat", "online", true);
    }
  }

  // Scan local sensors and timeouts
  handleTouch();
  handleServoTimeout();

  // Prevent watchdog timeouts
  yield();
}
