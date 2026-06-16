#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

extern bool relayState[2];
extern bool servoState[2];
extern bool lightState;
extern bool fanState;

void setupDevices();
void setRelay(int idx, bool state);
void toggleRelay(int idx);
void turnOnRelay(int idx);
void turnOffRelay(int idx);

void setServo(int idx, bool state);
void toggleServo(int idx);
void turnOnServo(int idx);
void turnOffServo(int idx);
void handleServoTimeout();

void setLight(bool state);
void setFan(bool state);
void toggleFan();
void turnOnFan();
void turnOffFan();

void shutdownAllDevices();

#endif
