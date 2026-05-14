#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

#define TOUCH D0
#define TOUCH_PIN_2 A0

void touchBegin();
void handleTouch(int& currentPage, unsigned long& lastPageUpdate);
void handleSecondaryTouch();
void singleTouch2();
void doubleTouch2();
void tripleTouch2();
void holdTouch2();

#endif