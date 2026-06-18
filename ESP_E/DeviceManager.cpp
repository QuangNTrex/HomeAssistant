#include "DeviceManager.h"
#include <Servo.h>
#include "MqttManager.h"

// Forward declaration for logging from ESP_E.ino
extern void log(const String& tag, const String& msg);

Servo lightServo;

bool buzzerState = false;
bool ledState = false;
bool servoLightState = false;
int motor1Speed = 0;
int motor2Speed = 0;
bool motor1State = false;
bool motor2State = false;

unsigned long servoTimer = 0;
bool servoActive = false;
const unsigned long SERVO_DETACH_DELAY = 600; // ms

void setupDevices() {
  // Set PWM range to 0-255 (standard duty cycle)
  analogWriteRange(255);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(MOTOR_1_PIN, OUTPUT);
  pinMode(MOTOR_2_PIN, OUTPUT);

  // Default initial states
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LIGHT_PIN, LOW);
  analogWrite(MOTOR_1_PIN, 0);
  analogWrite(MOTOR_2_PIN, 0);

  // Position servo initially to 110 degrees (OFF)
  lightServo.attach(SERVO_PIN);
  lightServo.write(110);
  servoActive = true;
  servoTimer = millis();

  log("DEVICE", "Hardware pins initialized. PWM range: 255.");
}

void setBuzzer(bool state) {
  if (buzzerState == state) return;
  buzzerState = state;
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  log("BUZZER", "Buzzer -> " + String(state ? "ON" : "OFF"));
  safePub("espE/buzzer/state", state ? "ON" : "OFF", true);
}

void toggleBuzzer() {
  setBuzzer(!buzzerState);
}

void setLED(bool state) {
  if (ledState == state) return;
  ledState = state;
  digitalWrite(LIGHT_PIN, state ? HIGH : LOW);
  log("LED", "LED -> " + String(state ? "ON" : "OFF"));
  safePub("espE/led/state", state ? "ON" : "OFF", true);
}

void toggleLED() {
  setLED(!ledState);
}

void setServoLight(bool state) {
  if (servoLightState == state) return;
  servoLightState = state;
  int angle = state ? 180 : 110;
  
  if (servoActive) {
    lightServo.detach();
    servoActive = false;
    delay(20);
  }

  lightServo.attach(SERVO_PIN);
  lightServo.write(angle);
  servoActive = true;
  servoTimer = millis();

  log("SERVO", "Light -> " + String(state ? "ON" : "OFF") + " (angle=" + String(angle) + ")");
  safePub("espE/servo1/state", state ? "ON" : "OFF", true);
}

void toggleServoLight() {
  setServoLight(!servoLightState);
}

void setMotorSpeed(int motorId, int speed) {
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  if (motorId == 1) {
    motor1Speed = speed;
    motor1State = (speed > 0);
    analogWrite(MOTOR_1_PIN, speed);
    log("MOTOR1", "Speed -> " + String(speed));
    safePub("espE/motor1/speed", String(speed).c_str(), true);
    safePub("espE/motor1/state", motor1State ? "ON" : "OFF", true);
  } else if (motorId == 2) {
    motor2Speed = speed;
    motor2State = (speed > 0);
    analogWrite(MOTOR_2_PIN, speed);
    log("MOTOR2", "Speed -> " + String(speed));
    safePub("espE/motor2/speed", String(speed).c_str(), true);
    safePub("espE/motor2/state", motor2State ? "ON" : "OFF", true);
  }
}

void setMotorState(int motorId, bool state) {
  if (motorId == 1) {
    if (motor1State == state) return;
    setMotorSpeed(1, state ? 255 : 0);
  } else if (motorId == 2) {
    if (motor2State == state) return;
    setMotorSpeed(2, state ? 255 : 0);
  }
}

void toggleMotorState(int motorId) {
  if (motorId == 1) {
    setMotorState(1, !motor1State);
  } else if (motorId == 2) {
    setMotorState(2, !motor2State);
  }
}

void handleServoTimeout() {
  if (servoActive && (millis() - servoTimer > SERVO_DETACH_DELAY)) {
    lightServo.detach();
    servoActive = false;
    log("SERVO", "Servo detached (movement complete)");
  }
}
