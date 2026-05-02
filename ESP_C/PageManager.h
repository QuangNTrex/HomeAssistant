#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <PubSubClient.h>

enum Page { PAGE_CLOCK = 0, PAGE_GREETING = 1, PAGE_SYSTEM = 2, PAGE_EVENT = 1000, PAGE_LOG = 3 };

class PageManager {
private:
  // References
  LiquidCrystal_I2C& lcd;
  PubSubClient& mqttClient;

  // Page state (chỉ PageManager quản lý)
  int lastPage = -1;
  unsigned long lastPageUpdate = 0;
  const unsigned long PAGE_INTERVAL = 8000; // 8s clock ↔ greeting

  // Event state (chỉ PageManager quản lý)
  String eventLine1 = "";
  String eventLine2 = "";
  unsigned long eventShownAt = 0;
  const unsigned long RETURN_TO_CLOCK = 30000;      // SYSTEM & LOG: 30s
  const unsigned long RETURN_TO_CLOCK_BY_EVENT = 5000;  // EVENT: 5s

  // Log state (chỉ PageManager quản lý)
  String logTag = "";
  String logMsg = "";
  unsigned long logTimestamp = 0;

  // Private methods for page rendering
  void pageClock(float lastTemp, float lastHum);
  void pageGreeting();
  void pageSystem();
  void pageEvent();
  void pageLog();

  // Helper function to format uptime
  String formatUptime(unsigned long milliseconds);

public:
  // Constructor
  PageManager(LiquidCrystal_I2C& lcdRef, PubSubClient& mqttRef);

  // Main handler
  void handleLCD(int& currentPage, unsigned long& lastLCDUpdate, const long LCD_INTERVAL, float lastTemp, float lastHum);

  // Public methods
  void showEvent(const String& l1, const String& l2, int& currentPage, unsigned long& lastLCDUpdate);
  void setLog(const String& tag, const String& msg);
  void notifyTimeoutPage(int page);
};

#endif