#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

#define TOUCH_PIN 0  // Chân D3 (GPIO0) on ESP8266

void touchBegin();
void handleTouch();

#endif // TOUCH_MANAGER_H
