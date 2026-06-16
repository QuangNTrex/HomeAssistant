#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

extern PubSubClient client;
extern bool wifiReconnectRequested;
extern unsigned long lastWifiReconnectAttempt;

void setup_wifi();
bool safePub(const char* topic, const char* payload, bool retained = false);
void reconnect();
void mqttLoop();
void mqttSetupCallback(void (*callback)(char*, byte*, unsigned int));

#endif
