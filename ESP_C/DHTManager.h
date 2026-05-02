#ifndef DHT_MANAGER_H
#define DHT_MANAGER_H

void dhtBegin();
void handleDHT(float& lastTemp, float& lastHum);

float computeHeatIndex(float t_c, float humidity);
float computeHeatIndex(float t, float h, float v);
float comfortIndex(float t, float h);

#endif