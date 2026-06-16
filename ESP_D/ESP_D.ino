#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>
#include <DHT.h>

#include "Config.h"
#include "LogHelper.h"
#include "TimeManager.h"
#include "MqttManager.h"
#include "DeviceManager.h"
#include "DHTManager.h"
#include "InputManager.h"

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[64];
  unsigned int copyLen = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, copyLen);
  msg[copyLen] = '\0';

  sysLogf("MQTT", "Received: %s = %s", topic, msg);

  if (strcmp(topic, "espD/relay1/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnRelay(0);
    else if (strcmp(msg, "OFF") == 0)    turnOffRelay(0);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(0);
  }
  else if (strcmp(topic, "espD/relay2/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnRelay(1);
    else if (strcmp(msg, "OFF") == 0)    turnOffRelay(1);
    else if (strcmp(msg, "TOGGLE") == 0) toggleRelay(1);
  }
  else if (strcmp(topic, "espD/servo1/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnServo(0);
    else if (strcmp(msg, "OFF") == 0)    turnOffServo(0);
    else if (strcmp(msg, "TOGGLE") == 0) toggleServo(0);
  }
  else if (strcmp(topic, "espD/servo2/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnServo(1);
    else if (strcmp(msg, "OFF") == 0)    turnOffServo(1);
    else if (strcmp(msg, "TOGGLE") == 0) toggleServo(1);
  }
  else if (strcmp(topic, "espD/fan/set") == 0) {
    if      (strcmp(msg, "ON") == 0)     turnOnFan();
    else if (strcmp(msg, "OFF") == 0)    turnOffFan();
    else if (strcmp(msg, "TOGGLE") == 0) toggleFan();
  }
  else {
    sysLogf("MQTT", "Unknown topic: %s", topic);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  sysLog("BOOT", "ESP_D starting...");

  setupDevices();
  setupInputs();

  sysLog("BOOT", "Pins initialized");

  setup_wifi();

  mqttSetupCallback(mqttCallback);
  sysLog("BOOT", "MQTT configured (buffer=512, keepalive=60s)");

  setupTime();

  char heapBuf[64];
  snprintf(heapBuf, sizeof(heapBuf), "Setup complete. Free heap: %u", ESP.getFreeHeap());
  sysLog("BOOT", heapBuf);

  setupDHT();
}

void loop() {
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    lastHeapLog = millis();
    char heapBuf[128];
    snprintf(heapBuf, sizeof(heapBuf), "Free heap: %u bytes | WiFi RSSI: %d dBm | MQTT: %s", ESP.getFreeHeap(), WiFi.RSSI(), client.connected() ? "OK" : "DISCONNECTED");
    sysLog("HEAP", heapBuf);
  }
  // publish uptime every 30 seconds
  static unsigned long lastUptimePub = 0;
  if (millis() - lastUptimePub > 30000) {
    lastUptimePub = millis();
    char uptimeStr[16];
    snprintf(uptimeStr, sizeof(uptimeStr), "%lu", millis());
    safePub("espD/uptime", uptimeStr);
  }

  mqttLoop();

  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();
  handleDHT();
}

