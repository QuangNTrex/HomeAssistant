#define MQTT_MAX_PACKET_SIZE 512

#include "MqttManager.h"
#include <ESP8266WiFi.h>
#include <Arduino.h>

// Forward declaration for logging in main
extern void log(const String& tag, const String& msg);

extern PubSubClient client;

extern String ESPD_RELAY1_STATE;
extern String ESPD_RELAY2_STATE;
extern String ESPD_SERVO1_STATE;
extern String ESPD_SERVO2_STATE;
extern String ESPD_FAN_STATE;
extern String ESP_TEMP;
extern String ESP_HUM;

const char* mqtt_server = "192.168.0.100";
const int   mqtt_port   = 1883;

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

void mqttBegin(MqttCallback callback) {
  client.setBufferSize(512);
  client.setKeepAlive(60);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

bool mqttIsConnected() {
  return client.connected();
}

void mqttLoop() {
  if (client.connected()) {
    client.loop();
  }
}

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

bool mqttReconnect() {
  if (client.connected()) return true;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return false;
  lastReconnectAttempt = now;

  log("MQTT", "Attempting reconnect to " + String(mqtt_server) + "...");

  if (client.connect("espC", nullptr, nullptr, "espC/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    client.publish("espC/status", "online", true);
    client.subscribe("espC/relay1/set");
    client.subscribe("espC/relay2/set");
    client.subscribe("espC/relay3/set");
    client.subscribe("espC/lcd/backlight/set");

    client.subscribe("espD/relay1/state");
    client.subscribe("espD/relay2/state");
    client.subscribe("espD/servo1/state");
    client.subscribe("espD/servo2/state");
    client.subscribe("espD/fan/state");
    client.subscribe("espD/temp");
    client.subscribe("espD/hum");

    log("MQTT", "Subscribed to relay topics");
    return true;
  }

  log("MQTT", "Failed, rc=" + String(client.state()) + " — retry in " + String(RECONNECT_INTERVAL/1000) + "s");
  // client.state() codes:
  // -4: timeout, -3: connection lost, -2: connect failed, -1: disconnected
  // 1: bad protocol, 2: bad client ID, 3: server unavailable, 4: bad credentials, 5: unauthorized
  return false;
}
