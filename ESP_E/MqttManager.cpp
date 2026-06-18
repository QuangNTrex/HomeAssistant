#define MQTT_MAX_PACKET_SIZE 512

#include "MqttManager.h"
#include <ESP8266WiFi.h>
#include <Arduino.h>

// Forward declaration for logging in main
extern void log(const String& tag, const String& msg);

extern PubSubClient client;

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

  // Establish connection with Last Will and Testament (LWT)
  if (client.connect("espE", nullptr, nullptr, "espE/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    
    // Announce online state
    client.publish("espE/status", "online", true);

    // Subscribe to controlling topics
    client.subscribe("espE/buzzer/set");
    client.subscribe("espE/led/set");
    client.subscribe("espE/servo1/set");
    client.subscribe("espE/motor1/set");
    client.subscribe("espE/motor2/set");

    log("MQTT", "Subscribed to ESP_E control topics");
    return true;
  }

  log("MQTT", "Failed, rc=" + String(client.state()) + " — retry in " + String(RECONNECT_INTERVAL / 1000) + "s");
  return false;
}
