#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

// GPIO mapping for NodeMCU ESP8266
#define SERVO_PIN      5  // D1 (GPIO5)
#define LIGHT_PIN      4  // D2 (GPIO4) - LED indicator
#define MOTOR_1_PIN   14  // D5 (GPIO14) - DC Motor 1
#define MOTOR_2_PIN   12  // D6 (GPIO12) - DC Motor 2
#define BUZZER_PIN    13  // D7 (GPIO13) - Buzzer

// Export states
extern bool buzzerState;
extern bool ledState;
extern bool servoLightState;
extern int motor1Speed;
extern int motor2Speed;
extern bool motor1State;
extern bool motor2State;

// Initialize hardware pins
void setupDevices();

// Buzzer controls
void setBuzzer(bool state);
void toggleBuzzer();

// LED controls
void setLED(bool state);
void toggleLED();

// Servo light controls (angle 180 = ON, 110 = OFF)
void setServoLight(bool state);
void toggleServoLight();

// Motor speed/state controls (0 to 255)
void setMotorSpeed(int motorId, int speed);
void setMotorState(int motorId, bool state);
void toggleMotorState(int motorId);

// Detach servo after movement is complete
void handleServoTimeout();

#endif // DEVICE_MANAGER_H
