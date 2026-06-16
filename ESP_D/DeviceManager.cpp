#include "DeviceManager.h"
#include "Config.h"
#include "LogHelper.h"
#include "MqttManager.h"
#include <Servo.h>

Servo servo1, servo2;

bool relayState[2] = {false, false};
bool servoState[2] = {false, false};
bool lightState    = false;
bool fanState      = false;

unsigned long servoTimer[2]  = {0, 0};
bool          servoActive[2] = {false, false};

void setupDevices() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LIGHT_PIN,  OUTPUT);
  pinMode(FAN_PIN,    OUTPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  setServo(0, false);
  setServo(1, false);
  
  digitalWrite(LIGHT_PIN,  LIGHT_OFF);
  analogWrite(FAN_PIN,    FAN_OFF_SPEED);
}

void setRelay(int idx, bool state) {
  if (relayState[idx] == state) {
    sysLogf("RELAY", "relay%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }
  relayState[idx] = state;
  int pin = (idx == 0) ? RELAY1_PIN : RELAY2_PIN;
  digitalWrite(pin, state ? HIGH : LOW);
  sysLogf("RELAY", "relay%d → %s", idx + 1, state ? "ON" : "OFF");

  char topic[24];
  snprintf(topic, sizeof(topic), "espD/relay%d/state", idx + 1);
  safePub(topic, state ? "ON" : "OFF", true);
}

void toggleRelay(int idx)  { setRelay(idx, !relayState[idx]); }
void turnOnRelay(int idx)  { setRelay(idx, true);  }
void turnOffRelay(int idx) { setRelay(idx, false); }

void setServo(int idx, bool state) {
  if (servoState[idx] == state) {
    sysLogf("SERVO", "servo%d already %s, skip", idx + 1, state ? "ON" : "OFF");
    return;
  }

  if (servoActive[idx]) {
    sysLogf("SERVO", "servo%d busy (moving), command queued/ignored", idx + 1);
    return;
  }

  servoState[idx]  = state;
  int angle        = state ? SERVO_ON_ANGLE : SERVO_OFF_ANGLE;

  sysLogf("SERVO", "servo%d → %s (angle=%d)", idx + 1, state ? "ON" : "OFF", angle);

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
      sysLogf("SERVO", "servo%d detached (movement complete)", i + 1);
    }
  }
}

void setLight(bool state) {
  if (lightState == state) return;
  lightState = state;
  digitalWrite(LIGHT_PIN, state ? LIGHT_ON : LIGHT_OFF);
  safePub("espD/light/state", state ? "ON" : "OFF", true);
  sysLog("LIGHT", state ? "ON" : "OFF");
}

void setFan(bool state) {
  if (fanState == state) {
    sysLogf("FAN", "fan already %s, skip", state ? "ON" : "OFF");
    return;
  }

  fanState = state;
  int speed = state ? FAN_ON_SPEED : FAN_OFF_SPEED;
  analogWrite(FAN_PIN, speed);
  safePub("espD/fan/state", state ? "ON" : "OFF", true);
  safePub("espD/fan/speed", state ? "1024" : "0", true);
  sysLogf("FAN", "fan → %s (speed=%d)", state ? "ON" : "OFF", speed);
}

void toggleFan() { setFan(!fanState); }
void turnOnFan()  { setFan(true); }
void turnOffFan() { setFan(false); }

void shutdownAllDevices() {
  sysLog("SYSTEM", "SHUTDOWN ALL devices triggered");
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
  sysLog("SYSTEM", "Shutdown complete");
}
