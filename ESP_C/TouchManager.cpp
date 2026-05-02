#include "TouchManager.h"
#include <LiquidCrystal_I2C.h>
#include "PageManager.h"

extern LiquidCrystal_I2C lcd;
extern PageManager pageManager;
extern bool relayState[3];

extern void log(const String& tag, const String& msg);
extern void toggleBacklight();
extern void setBacklight(bool state);
extern void toggleRelay(int index);
extern void showEvent(const String& l1, const String& l2);

enum TouchPhase { TOUCH_IDLE, TOUCH_COUNTING, TOUCH_HOLDING };

static TouchPhase touchPhase = TOUCH_IDLE;
static bool lastTouchState = LOW;
static int touchCount = 0;
static unsigned long touchStartTime = 0;
static unsigned long lastTouchTime = 0;

void touchBegin() {
  pinMode(TOUCH, INPUT_PULLUP);
}

static void handleHold() {
  toggleBacklight();
  log("TOUCH", "HOLD detected → TOGGLE backlight");
}

void handleTouch(int& currentPage, unsigned long& lastPageUpdate) {
  bool currentState = digitalRead(TOUCH);
  unsigned long now = millis();

  if (lastTouchState == LOW && currentState == HIGH) {
    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING && (now - lastTouchTime < 500)) {
      touchCount++;
      log("TOUCH", "Multi-tap count: " + String(touchCount));
    } else {
      touchCount = 1;
      touchPhase = TOUCH_COUNTING;
      log("TOUCH", "New tap, count: 1");
    }
    lastTouchTime = now;
  }

  if (currentState == HIGH
      && touchPhase == TOUCH_COUNTING
      && touchCount == 1
      && (now - touchStartTime > 2000)) {

    handleHold();
    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
  }

  if (lastTouchState == HIGH && currentState == LOW) {
    if (touchPhase == TOUCH_HOLDING) {
      log("TOUCH", "Released from HOLD → IDLE");
      touchPhase = TOUCH_IDLE;
    }
  }

  if (touchPhase == TOUCH_COUNTING
      && currentState == LOW
      && (now - lastTouchTime > 500)) {

    log("TOUCH", "Execute tap action, count=" + String(touchCount));

    if (touchCount == 1) {
        
      int next = (currentPage == PAGE_EVENT)
                ? PAGE_CLOCK
                : (currentPage + 1) % 4;
      currentPage    = next;
      lastPageUpdate = millis();
      pageManager.notifyTimeoutPage(next);
      lcd.clear();
      setBacklight(true);
      log("TOUCH", "1 tap → page " + String(currentPage));
    }
    else if (touchCount == 2) {
      toggleRelay(0);
      log("TOUCH", "2 tap → toggle relay1 (local)");
      showEvent("Tap Relay 1", relayState[0] ? "Turned ON" : "Turned OFF");
    }
    else if (touchCount == 3) {
      toggleRelay(1);
      log("TOUCH", "3 tap → toggle relay2 (man hinh)");
      showEvent("Tap Relay 2", relayState[1] ? "Turned ON" : "Turned OFF");
    }
    else {
      log("TOUCH", "No action for count=" + String(touchCount));
    }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}
