#ifndef DHT_MANAGER_H
#define DHT_MANAGER_H

#include <Arduino.h>

void dhtBegin();
void handleDHT();
float getTemperature();
float getHumidity();

#endif
