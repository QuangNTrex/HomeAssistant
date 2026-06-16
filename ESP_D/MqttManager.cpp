#include "MqttManager.h"
#include "Config.h"
#include "LogHelper.h"

WiFiClient   espClient;
PubSubClient client(espClient);

unsigned long lastReconnectAttempt = 0;
unsigned long lastWifiReconnectAttempt = 0;
bool wifiReconnectRequested = false;

void setup_wifi() {
  sysLog("WIFI", "Starting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 15000) {
      sysLog("WIFI", "Timeout! Continue without WiFi...");
      return;
    }
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "Connected. IP: %s", WiFi.localIP().toString().c_str());
  sysLog("WIFI", buf);
}

bool safePub(const char* topic, const char* payload, bool retained) {
  if (!client.connected()) {
    char buf[96];
    snprintf(buf, sizeof(buf), "Publish SKIPPED (disconnected): %s", topic);
    sysLog("MQTT", buf);
    return false;
  }

  bool ok = client.publish(topic, payload, retained);
  char buf[128];
  snprintf(buf, sizeof(buf), "Publish %s = %s %s", topic, payload, ok ? "OK" : "FAIL");
  sysLog("MQTT", buf);
  return ok;
}

void reconnect() {
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  char buf[96];
  snprintf(buf, sizeof(buf), "Attempting reconnect to %s...", mqtt_server);
  sysLog("MQTT", buf);

  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    sysLog("MQTT", "Connected!");
    client.publish("espD/status", "online", true);
    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");
    client.subscribe("espD/fan/set");
    sysLog("MQTT", "Subscribed to all topics");
  } else {
    snprintf(buf, sizeof(buf), "Failed, rc=%d — retry in %lu s", client.state(), RECONNECT_INTERVAL / 1000);
    sysLog("MQTT", buf);
  }
}

void mqttLoop() {
  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    bool firstRetry = !wifiReconnectRequested;
    if (firstRetry || now - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      lastWifiReconnectAttempt = now;
      wifiReconnectRequested = true;
      sysLog("WIFI", firstRetry ? "WiFi lost, starting reconnect..." : "WiFi reconnect retry...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
    return;
  }
  wifiReconnectRequested = false;

  if (!client.connected()) reconnect();
  client.loop();
}

void mqttSetupCallback(void (*callback)(char*, byte*, unsigned int)) {
  client.setBufferSize(512);
  client.setKeepAlive(60);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}
