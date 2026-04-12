#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

const char* ssid = "Test";
const char* password = "24082002";

// Loại cảm biến
#define DHTTYPE22 DHT22
#define DHTTYPE11 DHT11

// Chân kết nối
#define DHTPIN1 13   // DHT22 #1
#define DHTPIN2 14   // DHT22 #2
#define DHTPIN3 15   // DHT11

// Khởi tạo cảm biến
DHT dht1(DHTPIN1, DHTTYPE22);
DHT dht2(DHTPIN2, DHTTYPE22);
DHT dht3(DHTPIN3, DHTTYPE11);

WebServer server(80);

void handleRoot() {
  // Đọc dữ liệu
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();

  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();

  float t3 = dht3.readTemperature();
  float h3 = dht3.readHumidity();

  // HTML
  String html = "<html><head><meta http-equiv='refresh' content='3'></head><body>";
  html += "<h2>ESP32-CAM - 3 Sensors</h2>";

  html += "<h3>DHT22 #1 (GPIO13)</h3>";
  html += "Temp: " + String(t1) + " C<br>";
  html += "Humidity: " + String(h1) + " %<br>";

  html += "<h3>DHT22 #2 (GPIO14)</h3>";
  html += "Temp: " + String(t2) + " C<br>";
  html += "Humidity: " + String(h2) + " %<br>";

  html += "<h3>DHT11 (GPIO15)</h3>";
  html += "Temp: " + String(t3) + " C<br>";
  html += "Humidity: " + String(h3) + " %<br>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(9600);

  dht1.begin();
  dht2.begin();
  dht3.begin();

  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}