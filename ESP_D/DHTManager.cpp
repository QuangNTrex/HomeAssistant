#include "DHTManager.h"
#include <DHT.h>

extern void log(const char* tag, const char* msg);
extern void logf(const char* tag, const char* fmt, ...);
extern bool safePub(const char* topic, const char* payload, bool retained = false);

#define DHT_PIN    D3
#define DHT_TYPE   DHT22

static DHT dht(DHT_PIN, DHT_TYPE);

static float temperature = 0;
static float humidity = 0;

static unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

float getTemperature() { return temperature; }
float getHumidity() { return humidity; }

// ============================================================
//  HEAT INDEX & COMFORT INDEX CALCULATIONS
// ============================================================
static float computeHeatIndex(float t_c, float humidity) {
  float t = (t_c * 1.8) + 32.0;
  float hi;

  if (t < 80.0) {
    hi = 0.5 * (t + 61.0 + ((t - 68.0) * 1.2) + (humidity * 0.094));
  } 
  else {
    hi = -42.379 + (2.04901523 * t) + (10.14333127 * humidity) 
         - (0.22475541 * t * humidity) - (0.00683783 * t * t) 
         - (0.05481717 * humidity * humidity) + (0.00122874 * t * t * humidity) 
         + (0.00085282 * t * humidity * humidity) - (0.00000199 * t * t * humidity * humidity);

    if ((humidity < 13.0) && (t >= 80.0) && (t <= 112.0)) {
      float adj = ((13.0 - humidity) / 4.0) * sqrt((17.0 - abs(t - 95.0)) / 17.0);
      hi -= adj;
    } 
    else if ((humidity > 85.0) && (t >= 80.0) && (t <= 87.0)) {
      float adj = ((humidity - 85.0) / 10.0) * ((87.0 - t) / 5.0);
      hi += adj;
    }
  }

  return (hi - 32.0) / 1.8;
}

static float comfortIndex(float t, float h) {
  float cool    = 10.0;
  float comfort = 25.0;
  float hot     = 45.0;

  float base = (t >= 15 && h >= 40) ? computeHeatIndex(t, h) : t;
  float ci;

  if (base <= cool) {
    ci = 5.0 * (base - cool) / (comfort - cool);
  }
  else if (base <= comfort) {
    ci = 5.0 * (base - cool) / (comfort - cool);
  }
  else if (base <= hot) {
    ci = 5.0 + 5.0 * (base - comfort) / (hot - comfort);
  }
  else {
    ci = 5.0 + 5.0 * (base - comfort) / (hot - comfort);
  }

  return ci;
}

void dhtBegin() {
  dht.begin();
}

void handleDHT() {
  if (millis() - lastDHTRead < DHT_INTERVAL) return;
  lastDHTRead = millis();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    log("DHT", "Read failed");
    return;
  }

  temperature = t;
  humidity = h;

  char tempStr[8];
  char humStr[8];
  snprintf(tempStr, sizeof(tempStr), "%.1f", t);
  snprintf(humStr, sizeof(humStr), "%.1f", h);

  safePub("espD/temp", tempStr, true);
  safePub("espD/hum", humStr, true);

  float hi = computeHeatIndex(t, h);
  float ci = comfortIndex(t, h);

  char hiStr[8];
  char ciStr[8];
  snprintf(hiStr, sizeof(hiStr), "%.2f", hi);
  snprintf(ciStr, sizeof(ciStr), "%.2f", ci);

  safePub("espD/heat_index", hiStr, true);
  safePub("espD/comfort_index", ciStr, true);

  logf("DHT", "T=%s°C H=%s%% HI=%s CI=%s", tempStr, humStr, hiStr, ciStr);
}
