#include "TouchManager.h"
#include <LiquidCrystal_I2C.h>
#include "PageManager.h"

extern LiquidCrystal_I2C lcd;
extern PageManager pageManager;
extern bool relayState[3];
extern bool lcdBacklight;
extern unsigned long BACKLIGHT_SET_BY_TOUCH_AT;

extern void log(const String& tag, const String& msg);
extern void toggleBacklight();
extern void setBacklight(bool state);
extern void toggleRelay(int index);
extern void showEvent(const String& l1, const String& l2);

enum TouchPhase {
  TOUCH_IDLE,
  TOUCH_COUNTING,
  TOUCH_HOLDING
};

// ================= TOUCH 1 =================
static TouchPhase touchPhase = TOUCH_IDLE;
static bool lastTouchState = LOW;
static int touchCount = 0;
static unsigned long touchStartTime = 0;
static unsigned long lastTouchTime = 0;

// ================= TOUCH 2 =================
static TouchPhase touchPhase2 = TOUCH_IDLE;
static bool lastTouchState2 = LOW;
static int touchCount2 = 0;
static unsigned long touchStartTime2 = 0;
static unsigned long lastTouchTime2 = 0;

void touchBegin() {
  pinMode(TOUCH, INPUT_PULLUP);
}

// =====================================================
// TOUCH 1 HOLD
// =====================================================

static void handleHold() {
  toggleBacklight();
  BACKLIGHT_SET_BY_TOUCH_AT = millis();

  log("TOUCH", "HOLD detected → TOGGLE backlight");
}

// =====================================================
// TOUCH 2 ACTIONS
// =====================================================

void singleTouch2() {
  log("TOUCH2", "1 tap → toggle relay3");
}

void doubleTouch2() {
  log("TOUCH2", "2 tap → backlight ON");
}

void tripleTouch2() {
  log("TOUCH2", "3 tap → toggle backlight");
}

void holdTouch2() {
  log("TOUCH2", "HOLD detected");
}

// =====================================================
// MAIN TOUCH
// =====================================================

void handleTouch(int& currentPage, unsigned long& lastPageUpdate) {

  bool currentState = digitalRead(TOUCH);
  unsigned long now = millis();

  if (lastTouchState == LOW && currentState == HIGH) {

    touchStartTime = now;

    if (touchPhase == TOUCH_COUNTING &&
        (now - lastTouchTime < 500)) {

      touchCount++;

      log("TOUCH",
          "Multi-tap count: " + String(touchCount));

    } else {

      touchCount = 1;
      touchPhase = TOUCH_COUNTING;

      log("TOUCH", "New tap, count: 1");
    }

    lastTouchTime = now;
  }

  if (currentState == HIGH &&
      touchPhase == TOUCH_COUNTING &&
      touchCount == 1 &&
      (now - touchStartTime > 2000)) {

    handleHold();

    touchPhase = TOUCH_HOLDING;
    touchCount = 0;
  }

  if (lastTouchState == HIGH &&
      currentState == LOW) {

    if (touchPhase == TOUCH_HOLDING) {

      log("TOUCH",
          "Released from HOLD → IDLE");

      touchPhase = TOUCH_IDLE;
    }
  }

  if (touchPhase == TOUCH_COUNTING &&
      currentState == LOW &&
      (now - lastTouchTime > 500)) {

    log("TOUCH",
        "Execute tap action, count=" +
        String(touchCount));

    if (touchCount == 1) {

      int next = (currentPage == PAGE_EVENT)
                   ? PAGE_CLOCK
                   : (currentPage + 1) % 4;

      currentPage = next;

      lastPageUpdate = millis();

      pageManager.notifyTimeoutPage(next);

      lcd.clear();

      setBacklight(true);

      log("TOUCH",
          "1 tap → page " +
          String(currentPage));
    }
    else if (touchCount == 2) {

      toggleRelay(0);

      log("TOUCH",
          "2 tap → toggle relay1");

      showEvent(
        "Tap Relay 1",
        relayState[0]
          ? "Turned ON"
          : "Turned OFF"
      );
    }
    else if (touchCount == 3) {

      toggleRelay(1);

      log("TOUCH",
          "3 tap → toggle relay2");

      showEvent(
        "Tap Relay 2",
        relayState[1]
          ? "Turned ON"
          : "Turned OFF"
      );
    }

    touchCount = 0;
    touchPhase = TOUCH_IDLE;
  }

  lastTouchState = currentState;
}

// =====================================================
// SECONDARY TOUCH
// =====================================================

void handleSecondaryTouch() {
  bool currentState = analogRead(TOUCH_PIN_2) > 500 ? 1 : 0;
  unsigned long now = millis();

  if (lastTouchState2 == LOW &&
      currentState == HIGH) {

    touchStartTime2 = now;

    if (touchPhase2 == TOUCH_COUNTING &&
        (now - lastTouchTime2 < 500)) {

      touchCount2++;

      log("TOUCH2",
          "Multi-tap count: " +
          String(touchCount2));

    } else {

      touchCount2 = 1;
      touchPhase2 = TOUCH_COUNTING;

      log("TOUCH2",
          "New tap, count: 1");
    }

    lastTouchTime2 = now;
  }

  if (currentState == HIGH &&
      touchPhase2 == TOUCH_COUNTING &&
      touchCount2 == 1 &&
      (now - touchStartTime2 > 2000)) {

    holdTouch2();

    touchPhase2 = TOUCH_HOLDING;
    touchCount2 = 0;
  }

  if (lastTouchState2 == HIGH &&
      currentState == LOW) {

    if (touchPhase2 == TOUCH_HOLDING) {

      log("TOUCH2",
          "Released from HOLD → IDLE");

      touchPhase2 = TOUCH_IDLE;
    }
  }

  if (touchPhase2 == TOUCH_COUNTING &&
      currentState == LOW &&
      (now - lastTouchTime2 > 500)) {

    log("TOUCH2",
        "Execute tap action, count=" +
        String(touchCount2));

    if (touchCount2 == 1) {
      singleTouch2();
    }
    else if (touchCount2 == 2) {
      doubleTouch2();
    }
    else if (touchCount2 == 3) {
      tripleTouch2();
    }

    touchCount2 = 0;
    touchPhase2 = TOUCH_IDLE;
  }

  lastTouchState2 = currentState;
}