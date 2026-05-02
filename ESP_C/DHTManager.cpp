#include "DHTManager.h"
#include <Arduino.h>
#include <DHT.h>
#include <math.h>

// Forward declarations for functions in main
extern void log(const String& tag, const String& msg);
extern bool safePub(const char* topic, const char* payload);

#define DHTPIN  D4   // GPIO2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

unsigned long lastDHTRead = 0;
const long    DHT_INTERVAL = 5000;
const long    DOWN_HUMI    = 10;

float computeHeatIndex(float t_c, float humidity) {
  // 1. Chuyển đổi sang độ F
  float t = (t_c * 1.8) + 32.0;
  float hi;

  // 2. Tính toán Heat Index dựa trên ngưỡng 80 độ F (26.7 độ C)
  if (t < 80.0) {
    // Công thức đơn giản cho nhiệt độ thấp
    hi = 0.5 * (t + 61.0 + ((t - 68.0) * 1.2) + (humidity * 0.094));
  } 
  else {
    // Công thức hồi quy Rothfusz đầy đủ
    hi = -42.379 + (2.04901523 * t) + (10.14333127 * humidity) 
         - (0.22475541 * t * humidity) - (0.00683783 * t * t) 
         - (0.05481717 * humidity * humidity) + (0.00122874 * t * t * humidity) 
         + (0.00085282 * t * humidity * humidity) - (0.00000199 * t * t * humidity * humidity);

    // Hiệu chỉnh 1: Nếu độ ẩm thấp (< 13%) và nhiệt độ từ 80-112 độ F
    if ((humidity < 13.0) && (t >= 80.0) && (t <= 112.0)) {
      float adj = ((13.0 - humidity) / 4.0) * sqrt((17.0 - abs(t - 95.0)) / 17.0);
      hi -= adj;
    } 
    // Hiệu chỉnh 2: Nếu độ ẩm cao (> 85%) và nhiệt độ từ 80-87 độ F
    else if ((humidity > 85.0) && (t >= 80.0) && (t <= 87.0)) {
      float adj = ((humidity - 85.0) / 10.0) * ((87.0 - t) / 5.0);
      hi += adj;
    }
  }

  // 3. Chuyển đổi kết quả ngược lại độ C
  return (hi - 32.0) / 1.8;
}

float computeHeatIndex(float t, float h, float v) {
  // vapor pressure (e)
  float e = (h / 100.0) * 6.105 * exp((17.27 * t) / (237.7 + t));

  // Apparent Temperature (Steadman)
  float at = t + 0.33 * e - 0.70 * v - 4.0;

  return at;
}

float comfortIndex(float t, float h) {
  float cool    = 10.0;
  float comfort = 25.0;
  float hot     = 45.0;

  // chỉ dùng Heat Index khi đủ điều kiện
  float base = (t >= 15 && h >= 40) ? computeHeatIndex(t, h, 0) : t;

  float ci;

  // ❄️ Lạnh
  if (base <= cool) {
    ci = 0;
  }

  // 🌤️ Mát → dễ chịu
  else if (base <= comfort) {
    ci = 5.0 * (base - cool) / (comfort - cool);
  }

  // 🔥 Nóng
  else if (base <= hot) {
    ci = 5.0 + 5.0 * (base - comfort) / (hot - comfort);
  }

  // 🔴 Rất nóng
  else {
    ci = 10;
  }

  // clamp an toàn
  if (ci < 0) ci = 0;
  if (ci > 10) ci = 10;

  return ci;
}

void dhtBegin() {
  dht.begin();
}

void handleDHT(float& lastTemp, float& lastHum) {
  if (millis() - lastDHTRead < DHT_INTERVAL) return;
  lastDHTRead = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    log("DHT", "Read FAILED — check wiring/sensor");
    return;
  }

  // h = h - DOWN_HUMI;

  lastTemp = t;
  lastHum  = h;

  log("DHT", "Temp=" + String(t, 1) + "°C  Hum=" + String(h, 1) + "%");
  safePub("espC/temp", String(t, 1).c_str());
  safePub("espC/hum",  String(h, 1).c_str());

  // Tính toán và publish Heat Index và Comfort Index
  float hi = computeHeatIndex(t, h);
  float ci = comfortIndex(t, h);

  safePub("espC/heat_index", String(hi, 1).c_str());
  safePub("espC/comfort_index", String(ci, 1).c_str());

}
