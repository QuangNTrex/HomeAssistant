#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

void deviceBegin();

// Relay
void setRelay(int idx, bool state);
void toggleRelay(int idx);
void turnOnRelay(int idx);
void turnOffRelay(int idx);
bool getRelayState(int idx);

// Servo
void setServo(int idx, bool state);
void toggleServo(int idx);
void turnOnServo(int idx);
void turnOffServo(int idx);
void handleServoTimeout();
bool getServoState(int idx);

// Fan
void setFan(bool state);
void toggleFan();
void turnOnFan();
void turnOffFan();
bool getFanState();

// Light
void setLight(bool state);
bool getLightState();
bool getLightAutoOn();
void setLightAutoOn(bool state);

// System
void shutdownAllDevices();

#endif
