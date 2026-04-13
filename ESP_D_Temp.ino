#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <time.h>
#include <EEPROM.h>

// ================== TIME (NTP) ==================
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 7 * 3600;
const int   daylightOffset_sec = 0;

struct tm timeinfo;
bool timeReady = false;
String timeOfDay = "";

// ================== WIFI ==================
const char* ssid     = "Test";
const char* password = "24082002";

// ================== MQTT ==================
const char* mqtt_server = "192.168.0.100";

// ================== SERVO ANGLES ==================
const int SERVO_OFF_ANGLE = 100;
const int SERVO_ON_ANGLE  = 180;

// ================== PIN ==================
#define RELAY1_PIN D1
#define RELAY2_PIN D2
#define SERVO1_PIN D5
#define SERVO2_PIN D6
#define MOTION_PIN D7
#define TOUCH_PIN  D0
#define LIGHT_PIN  D4

// ================== EEPROM ==================
#define EEPROM_SIZE  8
#define EEPROM_MAGIC 0xAB

void saveStatesToEEPROM(bool s1, bool s2, bool r1, bool r2) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, EEPROM_MAGIC);
  EEPROM.write(1, s1 ? 1 : 0);
  EEPROM.write(2, s2 ? 1 : 0);
  EEPROM.write(3, r1 ? 1 : 0);
  EEPROM.write(4, r2 ? 1 : 0);
  EEPROM.commit();
  EEPROM.end();
  Serial.printf("[EEPROM] saved s1=%d s2=%d r1=%d r2=%d\n", s1, s2, r1, r2);
}

bool loadStatesFromEEPROM(bool &s1, bool &s2, bool &r1, bool &r2) {
  EEPROM.begin(EEPROM_SIZE);
  bool valid = (EEPROM.read(0) == EEPROM_MAGIC);
  if (valid) {
    s1 = EEPROM.read(1) == 1;
    s2 = EEPROM.read(2) == 1;
    r1 = EEPROM.read(3) == 1;
    r2 = EEPROM.read(4) == 1;
  }
  EEPROM.end();
  Serial.printf("[EEPROM] load valid=%d s1=%d s2=%d r1=%d r2=%d\n", valid, s1, s2, r1, r2);
  return valid;
}

// ================== OBJECTS ==================
WiFiClient   espClient;
PubSubClient client(espClient);
Servo servo1, servo2;

// ================== STATE ==================
bool relayState[2] = {false, false};
bool servoState[2] = {false, false};
bool lightState    = false;
bool lightAutoOn   = false;

unsigned long servoTimer[2] = {0, 0};
bool servoActive[2]         = {false, false};

const int LIGHT_ON  = LOW;
const int LIGHT_OFF = HIGH;

// ================== COMMAND QUEUE ==================
enum CmdTarget { CMD_RELAY1, CMD_RELAY2, CMD_SERVO1, CMD_SERVO2 };
enum CmdAction { CMD_ON, CMD_OFF, CMD_TOGGLE };
struct Command { CmdTarget target; CmdAction action; };

const int QUEUE_SIZE = 8;
Command   cmdQueue[QUEUE_SIZE];
int       cmdHead = 0, cmdTail = 0, cmdCount = 0;

bool enqueue(CmdTarget t, CmdAction a) {
  if (cmdCount >= QUEUE_SIZE) {
    Serial.println("[QUEUE] FULL — command dropped!");
    return false;
  }
  cmdQueue[cmdTail] = {t, a};
  cmdTail = (cmdTail + 1) % QUEUE_SIZE;
  cmdCount++;
  Serial.printf("[QUEUE] enqueue target=%d action=%d  (count=%d)\n", t, a, cmdCount);
  return true;
}

Command dequeue() {
  Command c = cmdQueue[cmdHead];
  cmdHead = (cmdHead + 1) % QUEUE_SIZE;
  cmdCount--;
  return c;
}

// ================== TOUCH STATE MACHINE ==================
enum TouchPhase { TOUCH_IDLE, TOUCH_COUNTING, TOUCH_HOLDING };
TouchPhase    touchPhase     = TOUCH_IDLE;
bool          lastTouchState = LOW;
int           touchCount     = 0;
unsigned long touchStartTime = 0;
unsigned long lastTouchTime  = 0;

// ================== MOTION ==================
bool lastMotionState = LOW;

// ================== MQTT RECONNECT ==================
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// ================== LOOP COUNTER (phát hiện loop chậm) ==================
unsigned long loopCount        = 0;
unsigned long lastLoopReport   = 0;

// ============================================================
//  RESET REASON — in ra nguyên nhân reset trước đó
// ============================================================
void printResetReason() {
  rst_info* ri = ESP.getResetInfoPtr();
  Serial.printf("[BOOT] Reset reason code: %d\n", ri->reason);
  switch (ri->reason) {
    case REASON_DEFAULT_RST:      Serial.println("[BOOT] Normal power-on"); break;
    case REASON_WDT_RST:          Serial.println("[BOOT] *** HARDWARE WATCHDOG RESET ***"); break;
    case REASON_EXCEPTION_RST:    Serial.printf ("[BOOT] *** EXCEPTION RESET — cause=%d epc1=0x%08x ***\n", ri->exccause, ri->epc1); break;
    case REASON_SOFT_WDT_RST:     Serial.println("[BOOT] *** SOFT WDT RESET (loop blocked > 3s) ***"); break;
    case REASON_SOFT_RESTART:     Serial.println("[BOOT] Software restart (ESP.restart)"); break;
    case REASON_DEEP_SLEEP_AWAKE: Serial.println("[BOOT] Deep sleep wake"); break;
    case REASON_EXT_SYS_RST:      Serial.println("[BOOT] External reset (EN pin)"); break;
    default:                      Serial.println("[BOOT] Unknown reset reason"); break;
  }
}

// ============================================================
//  TIME HELPERS
// ============================================================

bool isNightTime() {
  if (!timeReady || !getLocalTime(&timeinfo)) return false;
  int h = timeinfo.tm_hour, m = timeinfo.tm_min;
  return (h > 17 || (h == 17 && m >= 30) || h < 5);
}

String getTimeOfDay() {
  if (!getLocalTime(&timeinfo)) return "Unknown";
  int h = timeinfo.tm_hour, m = timeinfo.tm_min;
  String hStr = (h < 10 ? "0" : "") + String(h);
  String mStr = (m < 10 ? "0" : "") + String(m);
  String g;
  if      (h >= 5  && h <= 10)                          g = "morning";
  else if (h >= 11 && h <= 13)                          g = "midday";
  else if (h >= 14 && (h < 17 || (h == 17 && m < 30))) g = "afternoon";
  else                                                  g = "evening";
  return hStr + ":" + mStr + " Good " + g;
}

void updateTimeOfDay() {
  if (!timeReady) return;
  String newState = getTimeOfDay();
  if (newState != timeOfDay) {
    timeOfDay = newState;
    client.publish("espD/time_of_day", timeOfDay.c_str());
    Serial.printf("[TIME] %s\n", timeOfDay.c_str());
  }
}

// ============================================================
//  WIFI
// ============================================================

void setup_wifi() {
  Serial.printf("[WIFI] Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
    if (millis() - start > 10000) break;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WIFI] FAILED to connect (timeout 10s)");
  }
}

// ============================================================
//  RELAY
// ============================================================

const char* relayStateTopics[2] = {"espD/relay1/state", "espD/relay2/state"};

void setRelay(int idx, bool state) {
  Serial.printf("[RELAY%d] %s → %s\n", idx+1, relayState[idx]?"ON":"OFF", state?"ON":"OFF");
  relayState[idx] = state;
  digitalWrite(idx == 0 ? RELAY1_PIN : RELAY2_PIN, state ? HIGH : LOW);
  bool pub = client.publish(relayStateTopics[idx], state ? "ON" : "OFF");
  Serial.printf("[RELAY%d] publish %s\n", idx+1, pub ? "OK" : "FAIL");
  saveStatesToEEPROM(servoState[0], servoState[1], relayState[0], relayState[1]);
}

void toggleRelay(int idx) { setRelay(idx, !relayState[idx]); }
void turnOnRelay(int idx)  { setRelay(idx, true);             }
void turnOffRelay(int idx) { setRelay(idx, false);            }

// ============================================================
//  SERVO
// ============================================================

void handleServoTimeout() {
  for (int i = 0; i < 2; i++) {
    if (servoActive[i] && millis() - servoTimer[i] > 600) {
      Serial.printf("[SERVO%d] detach (timeout)\n", i+1);
      if (i == 0) servo1.detach();
      else        servo2.detach();
      servoActive[i] = false;
    }
  }
}

void setServo(int idx, bool state) {
  Serial.printf("[SERVO%d] setServo called: current=%s target=%s\n",
    idx+1, servoState[idx]?"ON":"OFF", state?"ON":"OFF");

  if (servoState[idx] == state) {
    Serial.printf("[SERVO%d] already in target state, skip\n", idx+1);
    return;
  }

  int currentAngle = servoState[idx] ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;
  int targetAngle  = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;
  Serial.printf("[SERVO%d] angle %d → %d\n", idx+1, currentAngle, targetAngle);

  // Không cho 2 servo hoạt động cùng lúc
  int otherIdx = 1 - idx;
  if (servoActive[otherIdx]) {
    Serial.printf("[SERVO%d] detach other servo first\n", idx+1);
    if (otherIdx == 0) servo1.detach();
    else               servo2.detach();
    servoActive[otherIdx] = false;
  }

  if (servoActive[idx]) {
    Serial.printf("[SERVO%d] detach self before re-attach\n", idx+1);
    if (idx == 0) servo1.detach();
    else          servo2.detach();
    servoActive[idx] = false;
    delay(20);
  }

  Serial.printf("[SERVO%d] attach pin D%d\n", idx+1, idx==0 ? 5 : 6);
  if (idx == 0) servo1.attach(SERVO1_PIN);
  else          servo2.attach(SERVO2_PIN);

  // Soft-start: di chuyển từng bước 5° để giảm current spike
  Serial.printf("[SERVO%d] soft-start begin\n", idx+1);
  int step = (targetAngle > currentAngle) ? 5 : -5;
  for (int a = currentAngle; ; a += step) {
    if ((step > 0 && a >= targetAngle) || (step < 0 && a <= targetAngle)) {
      a = targetAngle;
      if (idx == 0) servo1.write(a);
      else          servo2.write(a);
      break;
    }
    if (idx == 0) servo1.write(a);
    else          servo2.write(a);
    delay(8);
    yield();
  }
  Serial.printf("[SERVO%d] soft-start done, at %d°\n", idx+1, targetAngle);

  servoState[idx]  = state;
  servoActive[idx] = true;
  servoTimer[idx]  = millis();

  String topic = "espD/servo" + String(idx + 1) + "/state";
  bool pub = client.publish(topic.c_str(), state ? "ON" : "OFF");
  Serial.printf("[SERVO%d] publish state %s → %s\n", idx+1, state?"ON":"OFF", pub?"OK":"FAIL");

  saveStatesToEEPROM(servoState[0], servoState[1], relayState[0], relayState[1]);
}

void toggleServo(int idx) { setServo(idx, !servoState[idx]); }
void turnOnServo(int idx)  { setServo(idx, true);             }
void turnOffServo(int idx) { setServo(idx, false);            }

// ============================================================
//  LIGHT
// ============================================================

void setLight(bool state) {
  if (lightState == state) return;
  Serial.printf("[LIGHT] %s → %s\n", lightState?"ON":"OFF", state?"ON":"OFF");
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? LIGHT_ON : LIGHT_OFF);
  client.publish("espD/light/state", state ? "ON" : "OFF");
}

// ============================================================
//  SHUTDOWN ALL
// ============================================================

void shutdownAllDevices() {
  Serial.println("[SHUTDOWN] shutdownAllDevices called");
  turnOffRelay(0);
  turnOffRelay(1);
  turnOffServo(0);
  turnOffServo(1);
  client.publish("espC/relay1/set", "OFF");
  client.publish("espC/relay2/set", "OFF");
  client.publish("espC/relay3/set", "OFF");
}

// ============================================================
//  MQTT CALLBACK — chỉ enqueue, không thực thi trực tiếp
// ============================================================

void callback(char* topic, byte* payload, unsigned int length) {
  char msg[16] = {0};
  memcpy(msg, payload, min((unsigned int)(sizeof(msg) - 1), length));
  Serial.printf("[MQTT] recv [%s] = \"%s\"\n", topic, msg);

  CmdAction action;
  if      (strcmp(msg, "ON")     == 0) action = CMD_ON;
  else if (strcmp(msg, "OFF")    == 0) action = CMD_OFF;
  else if (strcmp(msg, "TOGGLE") == 0) action = CMD_TOGGLE;
  else {
    Serial.printf("[MQTT] unknown message \"%s\", ignored\n", msg);
    return;
  }

  if      (strcmp(topic, "espD/relay1/set") == 0) enqueue(CMD_RELAY1, action);
  else if (strcmp(topic, "espD/relay2/set") == 0) enqueue(CMD_RELAY2, action);
  else if (strcmp(topic, "espD/servo1/set") == 0) enqueue(CMD_SERVO1, action);
  else if (strcmp(topic, "espD/servo2/set") == 0) enqueue(CMD_SERVO2, action);
  else Serial.printf("[MQTT] unhandled topic \"%s\"\n", topic);
}

// ============================================================
//  PROCESS COMMAND QUEUE
// ============================================================

void processCommands() {
  if (cmdCount == 0) return;
  Command c = dequeue();
  Serial.printf("[QUEUE] execute target=%d action=%d  (remaining=%d)\n", c.target, c.action, cmdCount);

  switch (c.target) {
    case CMD_RELAY1:
      if      (c.action == CMD_ON)     turnOnRelay(0);
      else if (c.action == CMD_OFF)    turnOffRelay(0);
      else if (c.action == CMD_TOGGLE) toggleRelay(0);
      break;
    case CMD_RELAY2:
      if      (c.action == CMD_ON)     turnOnRelay(1);
      else if (c.action == CMD_OFF)    turnOffRelay(1);
      else if (c.action == CMD_TOGGLE) toggleRelay(1);
      break;
    case CMD_SERVO1:
      if      (c.action == CMD_ON)     turnOnServo(0);
      else if (c.action == CMD_OFF)    turnOffServo(0);
      else if (c.action == CMD_TOGGLE) toggleServo(0);
      break;
    case CMD_SERVO2:
      if      (c.action == CMD_ON)     turnOnServo(1);
      else if (c.action == CMD_OFF)    turnOffServo(1);
      else if (c.action == CMD_TOGGLE) toggleServo(1);
      break;
  }
}

// ============================================================
//  MQTT RECONNECT
// ============================================================

void reconnect() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  Serial.printf("[MQTT] reconnect attempt (state=%d)...\n", client.state());
  if (client.connect("espD", nullptr, nullptr, "espD/status", 0, true, "offline")) {
    Serial.println("[MQTT] connected");
    client.publish("espD/status", "online", true);
    client.subscribe("espD/relay1/set");
    client.subscribe("espD/relay2/set");
    client.subscribe("espD/servo1/set");
    client.subscribe("espD/servo2/set");

    // Publish lại toàn bộ state sau reconnect
    client.publish(relayStateTopics[0], relayState[0] ? "ON" : "OFF", true);
    client.publish(relayStateTopics[1], relayState[1] ? "ON" : "OFF", true);
    client.publish("espD/servo1/state", servoState[0] ? "ON" : "OFF", true);
    client.publish("espD/servo2/state", servoState[1] ? "ON" : "OFF", true);
    client.publish("espD/light/state",  lightState    ? "ON" : "OFF", true);
    Serial.println("[MQTT] states re-published");
  } else {
    Serial.printf("[MQTT] connect FAILED, state=%d\n", client.state());
  }
}

// ============================================================
//  MOTION
// ============================================================

void handleMotion() {
  bool motion = digitalRead(MOTION_PIN);
  if (motion != lastMotionState) {
    lastMotionState = motion;
    Serial.printf("[MOTION] %s  night=%d\n", motion?"detected":"cleared", isNightTime());
    client.publish("espD/motion", motion ? "1" : "0");
    if (motion == HIGH && isNightTime()) {
      lightAutoOn = true;
      setLight(true);
    } else if (motion == LOW && lightAutoOn) {
      lightAutoOn = false;
      setLight(false);
    }
  }
}

// ============================================================
//  TOUCH
// ============================================================

void handleTouch() {
  bool currentState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;
    if (touchPhase == TOUCH_COUNTING && now - lastTouchTime < 400) {
      touchCount++;
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
    }
    lastTouchTime = now;
    Serial.printf("[TOUCH] tap #%d\n", touchCount);
  }

  if (currentState == HIGH && touchPhase == TOUCH_COUNTING
      && touchCount == 1 && now - touchStartTime > 2000) {
    Serial.println("[TOUCH] HOLD detected → shutdownAllDevices");
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
    shutdownAllDevices();
  }

  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      touchPhase = TOUCH_IDLE;
      Serial.println("[TOUCH] released after hold");
    }
  }

  if (touchPhase == TOUCH_COUNTING && currentState == LOW
      && now - lastTouchTime > 400) {
    Serial.printf("[TOUCH] execute count=%d\n", touchCount);
    if      (touchCount == 1) toggleServo(0);
    else if (touchCount == 2) toggleServo(1);
    else if (touchCount == 3) toggleRelay(0);
    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(200); // chờ Serial ổn định
  Serial.println("\n\n========== ESP_D v4 boot ==========");
  printResetReason();
  Serial.printf("[SYS] Free heap: %u bytes\n", ESP.getFreeHeap());

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN,  OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(TOUCH_PIN,  INPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN,  LIGHT_OFF);

  // Khôi phục state từ EEPROM sau reset
  bool s1 = false, s2 = false, r1 = false, r2 = false;
  if (loadStatesFromEEPROM(s1, s2, r1, r2)) {
    Serial.println("[BOOT] Restoring state from EEPROM...");
    relayState[0] = r1; digitalWrite(RELAY1_PIN, r1 ? HIGH : LOW);
    relayState[1] = r2; digitalWrite(RELAY2_PIN, r2 ? HIGH : LOW);
    Serial.printf("[BOOT] relay1=%s relay2=%s\n", r1?"ON":"OFF", r2?"ON":"OFF");

    if (s1) {
      Serial.println("[BOOT] restoring servo1 → ON");
      servo1.attach(SERVO1_PIN);
      servo1.write(SERVO_ON_ANGLE);
      servoState[0]  = true;
      servoActive[0] = true;
      servoTimer[0]  = millis();
    }
    if (s2) {
      delay(100);
      Serial.println("[BOOT] restoring servo2 → ON");
      servo2.attach(SERVO2_PIN);
      servo2.write(SERVO_ON_ANGLE);
      servoState[1]  = true;
      servoActive[1] = true;
      servoTimer[1]  = millis();
    }
  } else {
    Serial.println("[BOOT] No valid EEPROM state, starting fresh");
  }

  Serial.println("[WIFI] Starting...");
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("[NTP] Syncing time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    if (millis() - start > 10000) {
      Serial.println("[NTP] timeout, continuing without time");
      break;
    }
  }
  if (getLocalTime(&timeinfo)) {
    Serial.printf("[NTP] Time OK: %02d:%02d:%02d\n",
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  timeReady = true;

  Serial.printf("[SYS] Free heap after setup: %u bytes\n", ESP.getFreeHeap());
  Serial.println("========== ESP_D ready ==========\n");
}

// ============================================================
//  LOOP HELPERS
// ============================================================

void sub_loop_time() {
  static unsigned long lastCheck = 0, lastSync = 0;
  if (millis() - lastCheck > 30000)  { updateTimeOfDay(); lastCheck = millis(); }
  if (millis() - lastSync > 3600000) {
    Serial.println("[NTP] re-sync");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    lastSync = millis();
  }
}

// Phát hiện loop bị chậm bất thường (>500ms giữa 2 vòng)
void checkLoopHealth() {
  static unsigned long lastLoopTime = 0;
  unsigned long now = millis();

  if (lastLoopTime > 0 && now - lastLoopTime > 500) {
    Serial.printf("[WARN] loop blocked for %lums!\n", now - lastLoopTime);
  }
  lastLoopTime = now;

  // In thống kê mỗi 10 giây
  loopCount++;
  if (now - lastLoopReport > 10000) {
    Serial.printf("[SYS] heap=%u  loops/10s=%lu  mqtt=%s  wifi=%s\n",
      ESP.getFreeHeap(),
      loopCount,
      client.connected() ? "OK" : "DISCONNECTED",
      WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED"
    );
    loopCount      = 0;
    lastLoopReport = now;
  }
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
  checkLoopHealth(); // phát hiện blocking sớm nhất có thể

  if (!client.connected()) reconnect();
  client.loop();

  processCommands();
  handleMotion();
  handleTouch();
  handleServoTimeout();
  sub_loop_time();

  yield();
}
