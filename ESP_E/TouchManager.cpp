#include "TouchManager.h"
#include "DeviceManager.h"

// Forward declaration for logging from ESP_E.ino
extern void log(const String& tag, const String& msg);

enum TouchPhase {
  TOUCH_IDLE,
  TOUCH_COUNTING,
  TOUCH_HOLDING
};

static TouchPhase touchPhase = TOUCH_IDLE;
static bool lastTouchState = LOW;
static int touchCount = 0;
static unsigned long touchStartTime = 0;
static unsigned long lastTouchTime = 0;

void touchBegin() {
  pinMode(TOUCH_PIN, INPUT);
}

void handleTouch() {
  bool currentState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // Detect touch onset (rising edge)
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && (now - lastTouchTime < 400)) {
      touchCount++;
      log("TOUCH", "Multi-tap count: " + String(touchCount));
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
      log("TOUCH", "Touch detected");
    }
    lastTouchTime = now;
  }

  // Detect long press while holding down (remains HIGH)
  if (currentState == HIGH && touchPhase == TOUCH_COUNTING && touchCount == 1) {
    if (now - touchStartTime > 1500) { // Held for more than 1.5 seconds
      log("TOUCH", "HOLD detected -> Turn ON Motor 1");
      setMotorSpeed(1, 255); // Turn ON DC Motor 1 to full speed
      touchPhase = TOUCH_HOLDING;
      touchCount = 0;
    }
  }

  // Detect touch release (falling edge)
  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      log("TOUCH", "Released from HOLD");
      touchPhase = TOUCH_IDLE;
    }
  }

  // Execute action after timeout has elapsed with no more taps
  if (touchPhase == TOUCH_COUNTING && currentState == LOW && (now - lastTouchTime > 400)) {
    log("TOUCH", "Execute tap action, count=" + String(touchCount));

    if (touchCount == 1) {
      log("TOUCH", "1 tap -> Toggle Servo Light");
      toggleServoLight();
    } else {
      log("TOUCH", "No action for count=" + String(touchCount));
    }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
  yield();
}
