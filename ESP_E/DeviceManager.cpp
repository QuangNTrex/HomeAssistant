#include "DeviceManager.h"
#include "MqttManager.h"
#include <Servo.h>

// Forward declaration for logging from ESP_E.ino
extern void log(const String &tag, const String &msg);

Servo lightServo;

const int MIN_MOTOR_PWM = 323;
const int MAX_MOTOR_PWM = 1023;

static int getMotorPwm(int level) {
  if (level < 1)
    level = 1;
  if (level > 5)
    level = 5;
  return MIN_MOTOR_PWM + (level - 1) * (MAX_MOTOR_PWM - MIN_MOTOR_PWM) / 4;
}

bool buzzerState = false;
bool ledState = false;
bool servoLightState = false;
int motor1Speed = 5;
int motor2Speed = 5;
bool motor1State = false;
bool motor2State = false;

unsigned long servoTimer = 0;
bool servoActive = false;
const unsigned long SERVO_DETACH_DELAY = 600; // ms

void setupDevices() {
  // Set PWM range to 0-1023 (standard duty cycle)
  analogWriteRange(1023);

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

  log("DEVICE", "Hardware pins initialized. PWM range: 1023.");
}

void setBuzzer(bool state) {
  if (buzzerState == state)
    return;
  buzzerState = state;
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  log("BUZZER", "Buzzer -> " + String(state ? "ON" : "OFF"));
  safePub("espE/buzzer/state", state ? "ON" : "OFF", true);
}

void toggleBuzzer() { setBuzzer(!buzzerState); }

void setLED(bool state) {
  if (ledState == state)
    return;
  ledState = state;
  digitalWrite(LIGHT_PIN, state ? HIGH : LOW);
  log("LED", "LED -> " + String(state ? "ON" : "OFF"));
  safePub("espE/led/state", state ? "ON" : "OFF", true);
}

void toggleLED() { setLED(!ledState); }

void setServoLight(bool state) {
  if (servoLightState == state)
    return;
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

  log("SERVO", "Light -> " + String(state ? "ON" : "OFF") +
                   " (angle=" + String(angle) + ")");
  safePub("espE/servo1/state", state ? "ON" : "OFF", true);
}

void toggleServoLight() { setServoLight(!servoLightState); }

void setMotorSpeed(int motorId, int speed) {
  if (speed < 1)
    speed = 1;
  if (speed > 5)
    speed = 5;

  int pwm = getMotorPwm(speed);

  if (motorId == 1) {
    motor1Speed = speed;
    motor1State = true;
    analogWrite(MOTOR_1_PIN, pwm);
    log("MOTOR1",
        "Speed Level -> " + String(speed) + " (PWM=" + String(pwm) + ")");
    safePub("espE/motor1/speed", String(speed).c_str(), true);
    safePub("espE/motor1/state", "ON", true);
  } else if (motorId == 2) {
    motor2Speed = speed;
    motor2State = true;
    analogWrite(MOTOR_2_PIN, pwm);
    log("MOTOR2",
        "Speed Level -> " + String(speed) + " (PWM=" + String(pwm) + ")");
    safePub("espE/motor2/speed", String(speed).c_str(), true);
    safePub("espE/motor2/state", "ON", true);
  }
}

void setMotorState(int motorId, bool state) {
  if (motorId == 1) {
    if (motor1State == state)
      return;
    motor1State = state;
    int pwm = state ? getMotorPwm(motor1Speed) : 0;
    analogWrite(MOTOR_1_PIN, pwm);
    log("MOTOR1", "State -> " + String(state ? "ON" : "OFF") +
                      " (PWM=" + String(pwm) + ")");
    safePub("espE/motor1/state", state ? "ON" : "OFF", true);
    safePub("espE/motor1/speed", String(motor1Speed).c_str(), true);
  } else if (motorId == 2) {
    if (motor2State == state)
      return;
    motor2State = state;
    int pwm = state ? getMotorPwm(motor2Speed) : 0;
    analogWrite(MOTOR_2_PIN, pwm);
    log("MOTOR2", "State -> " + String(state ? "ON" : "OFF") +
                      " (PWM=" + String(pwm) + ")");
    safePub("espE/motor2/state", state ? "ON" : "OFF", true);
    safePub("espE/motor2/speed", String(motor2Speed).c_str(), true);
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
