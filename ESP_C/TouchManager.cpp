#include "TouchManager.h"
#include <LiquidCrystal_I2C.h>
#include "PageManager.h"

extern LiquidCrystal_I2C lcd;
extern PageManager pageManager;
extern bool relayState[3];
extern bool lcdBacklight;
extern unsigned long BACKLIGHT_SET_BY_USER_AT;

extern String ESPD_RELAY1_SET;
extern String ESPD_RELAY2_SET;
extern String ESPD_SERVO1_SET;
extern String ESPD_SERVO2_SET;
extern String ESPD_FAN_SET;

extern long espDRelayStates[3];
extern long espDServoStates[3];
extern long espDFanState;

extern void log(const String& tag, const String& msg);
extern void toggleBacklight();
extern void setBacklight(bool state);
extern void toggleRelay(int index);
extern void showEvent(const String& l1, const String& l2);
extern bool safePub(const char* topic, const char* payload, bool retained = false);

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
  BACKLIGHT_SET_BY_USER_AT = millis();

  log("TOUCH", "HOLD detected → TOGGLE backlight");
}

// =====================================================
// TOUCH 2 ACTIONS
// =====================================================

//hello
void singleTouch2() {
  safePub(ESPD_RELAY1_SET.c_str(), "TOGGLE");
  log("TOUCH2", "1 tap → toggle relay1 (MQTT)");
  //showEvent("Tap ESP_D Relay 1", !espDRelayStates[0] ? "Turned ON" : "Turned OFF");
}

void doubleTouch2() {
  safePub(ESPD_SERVO1_SET.c_str(), "TOGGLE");
  log("TOUCH2", "2 tap → toggle servo 1 (MQTT)");
  //showEvent("Tap ESP_D Servo 1", !espDServoStates[0] ? "Turned ON" : "Turned OFF");
}

void tripleTouch2() {
  safePub(ESPD_SERVO2_SET.c_str(), "TOGGLE");
  log("TOUCH2", "3 tap → toggle servo 2 (MQTT)");
  //showEvent("Tap ESP_D Servo 2", !espDServoStates[1] ? "Turned ON" : "Turned OFF");
}

void holdTouch2() {
  safePub(ESPD_FAN_SET.c_str(), "TOGGLE");
  log("TOUCH2", "HOLD → toggle fan (MQTT)");
  //showEvent("Tap ESP_D Fan", !espDFanState ? "Turned ON" : "Turned OFF");
}

// =====================================================
// MAIN TOUCH
// =====================================================

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
                : (currentPage + 1) % 5;
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


// =====================================================
// SECONDARY TOUCH
// =====================================================


void handleSecondaryTouch() {
  // ================= PERFORMANCE FIX =================
  static unsigned long lastRead = 0;

  // chỉ đọc mỗi 30ms
  if (millis() - lastRead < 30) {
    return;
  }

  lastRead = millis();

  // ================= ADC HYSTERESIS =================
  static bool currentState = LOW;

  int raw = analogRead(TOUCH_PIN_2);

  // HIGH threshold
  if (!currentState && raw > 700) {
    currentState = HIGH;
  }
  // LOW threshold
  else if (currentState && raw < 300) {
    currentState = LOW;
  }

  unsigned long now = millis();

  // ================= EDGE DETECT =================

  if (lastTouchState2 == LOW &&
      currentState == HIGH) {

    touchStartTime2 = now;

    if (touchPhase2 == TOUCH_COUNTING &&
        (now - lastTouchTime2 < 500)) {

      touchCount2++;

    } else {

      touchCount2 = 1;
      touchPhase2 = TOUCH_COUNTING;
    }

    lastTouchTime2 = now;
  }

  // ================= HOLD =================

  if (currentState == HIGH &&
      touchPhase2 == TOUCH_COUNTING &&
      touchCount2 == 1 &&
      (now - touchStartTime2 > 2000)) {

    holdTouch2();

    touchPhase2 = TOUCH_HOLDING;
    touchCount2 = 0;
  }

  // ================= RELEASE =================

  if (lastTouchState2 == HIGH &&
      currentState == LOW) {

    if (touchPhase2 == TOUCH_HOLDING) {
      touchPhase2 = TOUCH_IDLE;
    }
  }

  // ================= EXECUTE ACTION =================

  if (touchPhase2 == TOUCH_COUNTING &&
      currentState == LOW &&
      (now - lastTouchTime2 > 500)) {

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

  // VERY IMPORTANT FOR ESP8266
  yield();
}