#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

#define TOUCH D0

void touchBegin();
void handleTouch(int& currentPage, unsigned long& lastPageUpdate);

#endif