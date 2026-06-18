#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>

using MqttCallback = void (*)(char* topic, byte* payload, unsigned int length);

void mqttBegin(MqttCallback callback);
bool mqttIsConnected();
void mqttLoop();
bool safePub(const char* topic, const char* payload, bool retain = false);
bool mqttReconnect();

#endif // MQTT_MANAGER_H
