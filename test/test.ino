#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// WiFi Configuration
const char* ssid     = "Test";
const char* password = "24082002";

ESP8266WebServer server(80);
Servo myServo;

// Pin mapping on NodeMCU/Wemos D1 Mini
const int SERVO_PIN  = D1;  // GPIO 5
const int BUZZER_PIN = D7;  // GPIO 13

bool servoState = false;
bool buzzerState = false;

// HTML page content stored in Flash Memory (PROGMEM)
const char HTML_CONTENT[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 Device Controller</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --accent-on: #10b981;
            --accent-off: #ef4444;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
            --servo-color: #6366f1;
            --buzzer-color: #f59e0b;
        }
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Outfit', sans-serif;
        }
        body {
            background-color: var(--bg-color);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            max-width: 600px;
            width: 100%;
            text-align: center;
        }
        header {
            margin-bottom: 40px;
        }
        h1 {
            font-size: 2.2rem;
            font-weight: 800;
            background: linear-gradient(135deg, #38bdf8, #818cf8);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 8px;
        }
        .subtitle {
            color: var(--text-secondary);
            font-size: 1rem;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr;
            gap: 24px;
        }
        @media(min-width: 480px) {
            .grid {
                grid-template-columns: 1fr 1fr;
            }
        }
        .card {
            background-color: var(--card-bg);
            border-radius: 20px;
            padding: 30px 20px;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.05);
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.4);
        }
        .card-icon {
            font-size: 3rem;
            margin-bottom: 15px;
        }
        .card-title {
            font-size: 1.3rem;
            font-weight: 600;
            margin-bottom: 10px;
        }
        .status-badge {
            display: inline-block;
            padding: 6px 16px;
            border-radius: 9999px;
            font-size: 0.85rem;
            font-weight: 600;
            margin-bottom: 25px;
            transition: all 0.3s ease;
        }
        .status-on {
            background-color: rgba(16, 185, 129, 0.15);
            color: var(--accent-on);
        }
        .status-off {
            background-color: rgba(239, 68, 68, 0.15);
            color: var(--accent-off);
        }
        .btn-group {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        .btn {
            border: none;
            padding: 12px 20px;
            font-size: 0.95rem;
            font-weight: 600;
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.2s ease;
            color: white;
        }
        .btn-on {
            background-color: var(--accent-on);
        }
        .btn-on:hover {
            background-color: #059669;
            box-shadow: 0 4px 12px rgba(16, 185, 129, 0.3);
        }
        .btn-off {
            background-color: var(--accent-off);
        }
        .btn-off:hover {
            background-color: #dc2626;
            box-shadow: 0 4px 12px rgba(239, 68, 68, 0.3);
        }
        footer {
            margin-top: 50px;
            font-size: 0.85rem;
            color: var(--text-secondary);
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>ESP8266 Web Controller</h1>
            <p class="subtitle">Điều khiển Động cơ Servo & Còi Buzzer</p>
        </header>

        <div class="grid">
            <!-- SERVO CARD -->
            <div class="card" style="border-top: 4px solid var(--servo-color);">
                <div class="card-icon" style="color: var(--servo-color);">⚙️</div>
                <div class="card-title">Động cơ Servo (D1)</div>
                <div id="servo-status" class="status-badge status-off">ĐANG TẮT (0°)</div>
                <div class="btn-group">
                    <button class="btn btn-on" onclick="setDevice('servo', 'on')">BẬT (180°)</button>
                    <button class="btn btn-off" onclick="setDevice('servo', 'off')">TẮT (0°)</button>
                </div>
            </div>

            <!-- BUZZER CARD -->
            <div class="card" style="border-top: 4px solid var(--buzzer-color);">
                <div class="card-icon" style="color: var(--buzzer-color);">🔊</div>
                <div class="card-title">Còi Buzzer (D7)</div>
                <div id="buzzer-status" class="status-badge status-off">ĐANG TẮT</div>
                <div class="btn-group">
                    <button class="btn btn-on" onclick="setDevice('buzzer', 'on')">BẬT</button>
                    <button class="btn btn-off" onclick="setDevice('buzzer', 'off')">TẮT</button>
                </div>
            </div>
        </div>

        <footer>
            ESP8266 Web Server &bull; Connected
        </footer>
    </div>

    <script>
        function setDevice(device, state) {
            fetch(`/${device}?state=${state}`)
                .then(response => response.json())
                .then(data => {
                    updateUI(device, data.state);
                })
                .catch(err => console.error("Lỗi gửi lệnh:", err));
        }

        function updateUI(device, state) {
            const statusEl = document.getElementById(`${device}-status`);
            if (device === 'servo') {
                if (state === 'ON') {
                    statusEl.innerText = 'ĐANG BẬT (180°)';
                    statusEl.className = 'status-badge status-on';
                } else {
                    statusEl.innerText = 'ĐANG TẮT (0°)';
                    statusEl.className = 'status-badge status-off';
                }
            } else if (device === 'buzzer') {
                if (state === 'ON') {
                    statusEl.innerText = 'ĐANG BẬT';
                    statusEl.className = 'status-badge status-on';
                } else {
                    statusEl.innerText = 'ĐANG TẮT';
                    statusEl.className = 'status-badge status-off';
                }
            }
        }

        // Khởi tạo trạng thái ban đầu khi load trang
        function initStates() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    updateUI('servo', data.servo);
                    updateUI('buzzer', data.buzzer);
                });
        }
        window.onload = initStates;
    </script>
</body>
</html>
)rawliteral";

// Request routing handlers
void handleRoot() {
  server.send_P(200, "text/html", HTML_CONTENT);
}

void handleStatus() {
  String json = "{\"servo\":\"" + String(servoState ? "ON" : "OFF") + 
                "\",\"buzzer\":\"" + String(buzzerState ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", json);
}

void handleServo() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      servoState = true;
      myServo.write(180);
      Serial.println("Servo: 180 degrees (ON)");
    } else {
      servoState = false;
      myServo.write(0);
      Serial.println("Servo: 0 degrees (OFF)");
    }
  }
  String json = "{\"device\":\"servo\",\"state\":\"" + String(servoState ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", json);
}

void handleBuzzer() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      buzzerState = true;
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("Buzzer: HIGH (ON)");
    } else {
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Buzzer: LOW (OFF)");
    }
  }
  String json = "{\"device\":\"buzzer\",\"state\":\"" + String(buzzerState ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nESP8266 starting up...");

  // Initialize hardware pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  myServo.attach(SERVO_PIN);
  myServo.write(0); // Default position

  // Start connecting to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // Try connecting for 10 seconds
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected successfully! IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Connection failed. Continuing to SoftAP Mode.");
  }

  // Create local Access Point as backup/alternative access
  WiFi.softAP("ESP8266-Control", "12345678");
  Serial.print("Access Point started. IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Setup server routing paths
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/servo", handleServo);
  server.on("/buzzer", handleBuzzer);

  // Start the server
  server.begin();
  Serial.println("HTTP Web Server Started.");
}

void loop() {
  server.handleClient();
  yield();
}
