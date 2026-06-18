#include "DeviceManager.h"
#include <Servo.h>

extern void log(const char* tag, const char* msg);
extern void logf(const char* tag, const char* fmt, ...);
extern bool safePub(const char* topic, const char* payload, bool retained = false);

// Pins
#define RELAY1_PIN D1
#define RELAY2_PIN D2
#define SERVO1_PIN D5
#define SERVO2_PIN D6
#define LIGHT_PIN  D4
#define FAN_PIN    D8

// Constants
const int SERVO_OFF_ANGLE = 100;
const int SERVO_ON_ANGLE  = 180;
const unsigned long SERVO_DETACH_DELAY = 400;

const int LIGHT_ON      = HIGH;
const int LIGHT_OFF     = LOW;
const int FAN_ON_SPEED  = 1024;
const int FAN_OFF_SPEED = 0;

// States
static bool relayState[2] = {false, false};
static bool servoState[2] = {false, false};
static bool lightState    = false;
static bool lightAutoOn   = false;
static bool fanState      = false;

static unsigned long servoTimer[2]  = {0, 0};
static bool          servoActive[2] = {false, false};

static Servo servo1, servo2;

void deviceBegin() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN,  OUTPUT);
  pinMode(FAN_PIN,    OUTPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(LIGHT_PIN,  LIGHT_OFF);
  analogWrite(FAN_PIN,    FAN_OFF_SPEED);

  setServo(0, false);
  setServo(1, false);
}

// ============================================================
//  RELAY
// ============================================================
bool getRelayState(int idx) {
  if (idx < 0 || idx >= 2) return false;
  return relayState[idx];
}

void setRelay(int idx, bool state) {
  if (idx < 0 || idx >= 2) return;

  if (relayState[idx] == state) {
    logf("RELAY", "relay%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }
  relayState[idx] = state;
  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);
  logf("RELAY", "relay%d → %s", idx + 1, state ? "ON" : "OFF");

  char topic[24];
  snprintf(topic, sizeof(topic), "espD/relay%d/state", idx + 1);
  safePub(topic, state ? "ON" : "OFF", true);
}

void toggleRelay(int idx)  { setRelay(idx, !relayState[idx]); }
void turnOnRelay(int idx)  { setRelay(idx, true);  }
void turnOffRelay(int idx) { setRelay(idx, false); }

// ============================================================
//  SERVO
// ============================================================
bool getServoState(int idx) {
  if (idx < 0 || idx >= 2) return false;
  return servoState[idx];
}

void setServo(int idx, bool state) {
  if (idx < 0 || idx >= 2) return;

  if (servoState[idx] == state) {
    logf("SERVO", "servo%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }

  if (servoActive[idx]) {
    logf("SERVO", "servo%d busy (moving), command queued/ignored", idx + 1);
    return;
  }

  servoState[idx]  = state;
  int angle        = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

  logf("SERVO", "servo%d → %s (angle=%d)", idx + 1, state ? "ON" : "OFF", angle);

  if (idx == 0) {
    servo1.attach(SERVO1_PIN);
    servo1.write(angle);
  } else {
    servo2.attach(SERVO2_PIN);
    servo2.write(angle);
  }

  servoActive[idx] = true;
  servoTimer[idx]  = millis();

  char topic[24];
  snprintf(topic, sizeof(topic), "espD/servo%d/state", idx + 1);
  safePub(topic, state ? "ON" : "OFF", true);
}

void toggleServo(int idx) { setServo(idx, !servoState[idx]); }
void turnOnServo(int idx)  { setServo(idx, true);  }
void turnOffServo(int idx) { setServo(idx, false); }

void handleServoTimeout() {
  for (int i = 0; i < 2; i++) {
    if (servoActive[i] && (millis() - servoTimer[i] > SERVO_DETACH_DELAY)) {
      if (i == 0) servo1.detach();
      else        servo2.detach();
      servoActive[i] = false;
      logf("SERVO", "servo%d detached (movement complete)", i + 1);
    }
  }
}

// ============================================================
//  FAN / MOTOR
// ============================================================
bool getFanState() {
  return fanState;
}

void setFan(bool state) {
  if (fanState == state) {
    logf("FAN", "fan already %s, skip", state ? "ON" : "OFF");
    return;
  }

  fanState = state;
  int speed = state ? FAN_ON_SPEED : FAN_OFF_SPEED;
  analogWrite(FAN_PIN, speed);
  safePub("espD/fan/state", state ? "ON" : "OFF", true);
  safePub("espD/fan/speed", state ? "1024" : "0", true);
  logf("FAN", "fan → %s (speed=%d)", state ? "ON" : "OFF", speed);
}

void toggleFan() { setFan(!fanState); }
void turnOnFan()  { setFan(true); }
void turnOffFan() { setFan(false); }

// ============================================================
//  LIGHT
// ============================================================
bool getLightState() {
  return lightState;
}

bool getLightAutoOn() {
  return lightAutoOn;
}

void setLightAutoOn(bool state) {
  lightAutoOn = state;
}

void setLight(bool state) {
  if (lightState == state) return;
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? LIGHT_ON : LIGHT_OFF);
  safePub("espD/light/state", state ? "ON" : "OFF", true);
  log("LIGHT", state ? "ON" : "OFF");
}

// ============================================================
//  SHUTDOWN ALL
// ============================================================
void shutdownAllDevices() {
  log("SYSTEM", "SHUTDOWN ALL devices triggered");
  turnOffRelay(0);
  turnOffRelay(1);
  turnOffServo(0);
  turnOffServo(1);
  setLight(false);
  setFan(false);

  safePub("espC/relay1/set", "OFF");
  safePub("espC/relay2/set", "OFF");
  safePub("espC/relay3/set", "OFF");
  safePub("espD/fan/set", "OFF");
  log("SYSTEM", "Shutdown complete");
}
