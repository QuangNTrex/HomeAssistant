#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

#define TOUCH_PIN D0 // Chân D0 (GPIO16) on ESP8266

void touchBegin();
void handleTouch();
void singleTouch();
void doubleTouch();
void tripleTouch();
void longTouch();

#endif // TOUCH_MANAGER_H
