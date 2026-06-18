#include "TouchManager.h"
#include "DeviceManager.h"

extern void log(const char* tag, const char* msg);
extern void logf(const char* tag, const char* fmt, ...);

#define TOUCH_KITCHEN A0
#define TOUCH_PIN  D0
const int ANALOG_READ_DELAY = 35;

enum TouchPhase { TOUCH_IDLE, TOUCH_COUNTING, TOUCH_HOLDING };
static TouchPhase    touchPhase     = TOUCH_IDLE;
static bool          lastTouchState = LOW;
static int           touchCount     = 0;
static unsigned long touchStartTime = 0;
static unsigned long lastTouchTime  = 0;
static long lastAnalogReadTime = 0;

void touchBegin() {
  pinMode(TOUCH_PIN, INPUT);
}

void handleTouch() {
  if (millis() - lastAnalogReadTime < ANALOG_READ_DELAY) return;
  lastAnalogReadTime = millis();  
  bool currentStateOfTouchKitchen = analogRead(TOUCH_KITCHEN) > 500;
  bool currentState = digitalRead(TOUCH_PIN) ? HIGH : currentStateOfTouchKitchen;

  unsigned long now = millis();

  // Phát hiện chạm (cạnh lên: LOW → HIGH)
  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && (now - lastTouchTime < 400)) {
      touchCount++;
      logf("TOUCH", "Multi-tap count: %d", touchCount);
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
      log("TOUCH", "New tap, count: 1");
    }
    lastTouchTime = now;
  }

  // Kiểm tra HOLD (giữ > 2s, single tap)
  if (currentState == HIGH
      && touchPhase == TOUCH_COUNTING
      && touchCount == 1
      && (now - touchStartTime > 2000)) {

    log("TOUCH", "HOLD detected → shutdownAllDevices");
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
    shutdownAllDevices();
  }

  // Phát hiện thả tay (cạnh xuống: HIGH → LOW)
  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      log("TOUCH", "Released from HOLD → IDLE");
      touchPhase = TOUCH_IDLE;
    }
  }

  // Timeout multi-tap: 400ms sau lần chạm cuối → thực thi
  if (touchPhase == TOUCH_COUNTING
      && currentState == LOW
      && (now - lastTouchTime > 400)) {

    logf("TOUCH", "Execute tap action, count=%d", touchCount);
    if      (touchCount == 1) { toggleServo(0); log("TOUCH", "1 tap → toggleServo1 (den chinh)"); }
    else if (touchCount == 2) { toggleServo(1); log("TOUCH", "2 tap → toggleServo2 (den bep)"); }
    else if (touchCount == 3) { toggleRelay(0); log("TOUCH", "3 tap → toggleRelay1"); }
    else { logf("TOUCH", "No action for count=%d", touchCount); }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}
