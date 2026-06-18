#include "MqttManager.h"
#include <ESP8266WiFi.h>

extern WiFiClient espClient;
extern PubSubClient client;
extern void log(const char* tag, const char* msg);
extern void logf(const char* tag, const char* fmt, ...);

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

bool safePub(const char* topic, const char* payload, bool retained) {
  if (!client.connected()) {
    char buf[96];
    snprintf(buf, sizeof(buf), "Publish SKIPPED (disconnected): %s", topic);
    log("MQTT", buf);
    return false;
  }

  bool ok = client.publish(topic, payload, retained);
  char buf[128];
  snprintf(buf, sizeof(buf), "Publish %s = %s %s", topic, payload, ok ? "OK" : "FAIL");
  log("MQTT", buf);
  return ok;
}

void mqttReconnect() {
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  char buf[96];
  snprintf(buf, sizeof(buf), "Attempting reconnect to %s...", mqtt_server);
  log("MQTT", buf);

  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    log("MQTT", "Connected!");
    client.publish("espD/status", "online", true);
    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");
    client.subscribe("espD/fan/set");
    log("MQTT", "Subscribed to all topics");
  } else {
    snprintf(buf, sizeof(buf), "Failed, rc=%d — retry in %lu s", client.state(), RECONNECT_INTERVAL / 1000);
    log("MQTT", buf);
  }
}
