#include <BleKeyboard.h>
#include <Preferences.h> // 1. THÊM THƯ VIỆN LƯU TRỮ

#define KEY_NUM_LOCK 0xDB

BleKeyboard bleKeyboard("Joy Gamer", "DIY Creator", 100);
Preferences preferences; // 2. KHỞI TẠO ĐỐI TƯỢNG PREFERENCES

const int LED_PIN = 2; 

const int PRESSED  = LOW;   
const int RELEASED = HIGH;  

// TAY CẦM 1 (Đọc trực tiếp bằng các chân vật lý trên ESP32)
const int JOY1_X = 36; const int JOY1_Y = 39; const int JOY1_K = 12;
const int JOY1_A = 32; const int JOY1_B = 33; const int JOY1_C = 25;
const int JOY1_D = 26; const int JOY1_E = 27; const int JOY1_F = 13; // NÚT CHUYỂN PROFILE

// TAY CẦM 2 (Xử lý hỗn hợp: Trục Y đọc trực tiếp, Trục X + Toàn bộ nút nhận qua Serial từ ESP8266)
const int JOY2_Y = 35; // Nhận tín hiệu Analog trục Y trực tiếp từ tay phụ qua 1 sợi dây riêng

// Cấu hình ngưỡng gạt của Joystick
const int JOY_THRESHOLD_LOW  = 600; 
const int JOY_THRESHOLD_HIGH = 3400; 

// Ngưỡng riêng cho ESP8266 (do dải ADC của ESP8266 là 10-bit: 0 - 1023)
const int ESP8266_JOY_LOW    = 200;
const int ESP8266_JOY_HIGH   = 800;

// Biến lưu trạng thái ảo của Tay cầm 2 sau khi giải mã từ Serial gửi về
int subX = 512;
int subA = 1, subB = 1, subC = 1, subD = 1, subE = 1, subF = 1, subK = 1;

// CẤU TRÚC PROFILE GAME (Đã thêm t2_e và t2_f)
struct GameProfile {
  // Hướng di chuyển (Joystick)
  uint8_t t1_left; uint8_t t1_right; uint8_t t1_up; uint8_t t1_down;
  uint8_t t2_left; uint8_t t2_right; uint8_t t2_up; uint8_t t2_down;

  // Các nút bấm hành động tương ứng từng chân phần cứng
  uint8_t t1_k; uint8_t t1_a; uint8_t t1_b; uint8_t t1_c; uint8_t t1_d; uint8_t t1_e;
  uint8_t t2_k; uint8_t t2_a; uint8_t t2_b; uint8_t t2_c; uint8_t t2_d; uint8_t t2_e; uint8_t t2_f;
};

// Profile 0: pr ađa
GameProfile profile0 = { 
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  0, 'w', 'd', 's', 'a', 0,
  0, KEY_UP_ARROW, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW, 0, 0
};

// Profile 1: bad ice
GameProfile profile1 = { 
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  0, 'q', 0, 0, 0, 'r',
  0, ' ', 0, 0, 0, 0, 0
};

// Profile 2: Songoku 2.5 (Đã cấu hình nút F thành phím gồng KEY_NUM_3)
GameProfile profile2 = { 
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  'l', 'k', 'o', 'i', 'j', 'u',
  KEY_NUM_3, KEY_NUM_2, KEY_NUM_6, KEY_NUM_5, KEY_NUM_1, KEY_NUM_4, KEY_NUM_LOCK
};

GameProfile activeProfiles[] = {profile0, profile1, profile2};
int currentProfileIndex = 0; 
const int TOTAL_PROFILES = sizeof(activeProfiles) / sizeof(activeProfiles[0]);

unsigned long lastScanTime = 0;    
unsigned long lastLedToggle = 0;    
int ledFlashesCount = 0;            
int ledState = LOW;

// Mảng lưu trạng thái nút
int lastButtonStates[40]; 
int lastSubButtonStates[8]; // Lưu trạng thái cũ của [A, B, C, D, E, F, K] từ ESP8266
bool isButtonInitialized = false; 

bool isLeftPressedGlobal[2]  = {false, false};
bool isRightPressedGlobal[2] = {false, false};
bool isUpPressedGlobal[2]    = {false, false};
bool isDownPressedGlobal[2]  = {false, false};

uint8_t lastLeftKeyGlobal[2]  = {0, 0};
uint8_t lastRightKeyGlobal[2] = {0, 0};
uint8_t lastUpKeyGlobal[2]    = {0, 0};
uint8_t lastDownKeyGlobal[2]  = {0, 0};

int lastButtonFState = RELEASED; 

void triggerLedBlink(int flashes);
void checkButton(int pin, uint8_t key);
void checkSubButton(int currentVirtualState, int &lastVirtualState, uint8_t key, const char* btnName);
void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown, int joyIndex, int threshLow, int threshHigh);

void setup() {
  Serial.begin(115200); 
  
  // Khởi tạo Serial2 để kết nối nhận data từ ESP8266: RX2 = GPIO16, TX2 = GPIO17
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  bleKeyboard.begin();
  pinMode(LED_PIN, OUTPUT);
  
  // Cấu hình các chân vật lý hiện có trên Tay 1 của ESP32
  int pins[] = {JOY1_K, JOY1_A, JOY1_B, JOY1_C, JOY1_D, JOY1_E, JOY1_F};
  for (int pin : pins) pinMode(pin, INPUT_PULLUP);
  
  // Khởi tạo trạng thái mảng nút phụ ảo (mặc định là nhả = RELEASED = 1)
  for (int i = 0; i < 8; i++) lastSubButtonStates[i] = RELEASED;

  // 3. ĐỌC PROFILE ĐÃ LƯU TỪ BỘ NHỚ KHỞI ĐỘNG
  preferences.begin("gamepad", false); // Mở không gian bộ nhớ tên "gamepad"
  currentProfileIndex = preferences.getInt("profile", 0); // Đọc biến "profile", mặc định = 0 nếu chưa từng lưu
  preferences.end(); // Đóng lại để giải phóng bộ nhớ

  // Kiểm tra an toàn phòng trường hợp dữ liệu rác ngoài phạm vi mảng
  if (currentProfileIndex >= TOTAL_PROFILES || currentProfileIndex < 0) {
    currentProfileIndex = 0;
  }

  Serial.print("--- KHI PHOI DONG: Da load Profile index: "); 
  Serial.println(currentProfileIndex);

  // Nháy LED báo hiệu profile hiện tại ngay khi khởi động
  triggerLedBlink(currentProfileIndex + 1); 
}

void loop() {
  unsigned long currentTime = millis();

  // 1. NHẬN VÀ GIẢI MÃ CHUỒI DATA TỪ ESP8266 (TAY PHỤ) VỚI CẤU HÌNH MỚI
  if (Serial2.available() > 0) {
    String data = Serial2.readStringUntil('\n');
    data.trim();
    
    if (data.length() > 0) {
      String values[8]; 
      int currentIndex = 0;
      int pos = 0;
      
      // Tách chuỗi bằng dấu phẩy ','
      while ((pos = data.indexOf(',')) != -1 && currentIndex < 7) {
        values[currentIndex] = data.substring(0, pos);
        data = data.substring(pos + 1);
        currentIndex++;
      }
      values[currentIndex] = data; // Phần tử cuối cùng (Nút K)

      // Cập nhật các biến trạng thái từ gói tin (TrụcX,A,B,C,D,E,F,K)
      subX = values[0].toInt();
      subA = values[1].toInt();
      subB = values[2].toInt();
      subC = values[3].toInt();
      subD = values[4].toInt();
      subE = values[5].toInt();
      subF = values[6].toInt();
      subK = values[7].toInt();
    }
  }

  // 2. LOGIC CHUYỂN PROFILE (Nút F trên Tay 1 kích hoạt chuyển nhanh)
  int currentButtonFState = digitalRead(JOY1_F);
  if (lastButtonFState == RELEASED && currentButtonFState == PRESSED) {
    currentProfileIndex = (currentProfileIndex + 1) % TOTAL_PROFILES;
    bleKeyboard.releaseAll(); 
    
    // 4. LƯU PROFILE MỚI VÀO BỘ NHỚ FLASH TỨC THÌ
    preferences.begin("gamepad", false);
    preferences.putInt("profile", currentProfileIndex); // Lưu giá trị mới vào key "profile"
    preferences.end();
    
    Serial.print("--- DA LUU PROFILE MOI: "); Serial.println(currentProfileIndex);

    triggerLedBlink(currentProfileIndex + 1); 
  }
  lastButtonFState = currentButtonFState; 

  if (ledFlashesCount > 0) {
    if (currentTime - lastLedToggle >= 150) { 
      lastLedToggle = currentTime;
      ledState = (ledState == LOW) ? HIGH : LOW;
      digitalWrite(LED_PIN, ledState);
      if (ledState == LOW) ledFlashesCount--; 
    }
  }

  // 3. QUÉT VÀ TRUYỀN PHÍM CHU KỲ 10MS
  if (currentTime - lastScanTime >= 10) {
    lastScanTime = currentTime;

    if (bleKeyboard.isConnected()) {
      GameProfile p = activeProfiles[currentProfileIndex]; 

      // --- XỬ LÝ TAY CẦM 1 (Quét phần cứng trực tiếp) ---
      checkJoystick(analogRead(JOY1_X), analogRead(JOY1_Y), p.t1_left, p.t1_right, p.t1_up, p.t1_down, 0, JOY_THRESHOLD_LOW, JOY_THRESHOLD_HIGH);
      checkButton(JOY1_K, p.t1_k); 
      checkButton(JOY1_A, p.t1_a); checkButton(JOY1_B, p.t1_b); checkButton(JOY1_C, p.t1_c);
      checkButton(JOY1_D, p.t1_d); checkButton(JOY1_E, p.t1_e); 

      // --- XỬ LÝ TAY CẦM 2 (Hỗn hợp nhận từ Serial + Analog Trục Y trực tiếp) ---
      checkJoystick(subX, analogRead(JOY2_Y), p.t2_left, p.t2_right, p.t2_up, p.t2_down, 1, ESP8266_JOY_LOW, ESP8266_JOY_HIGH);
      
      // Quét toàn bộ các nút bấm từ luồng dữ liệu của ESP8266 bao gồm cả nút E và F mới
      checkSubButton(subA, lastSubButtonStates[0], p.t2_a, "Sub_A");
      checkSubButton(subB, lastSubButtonStates[1], p.t2_b, "Sub_B");
      checkSubButton(subC, lastSubButtonStates[2], p.t2_c, "Sub_C");
      checkSubButton(subD, lastSubButtonStates[3], p.t2_d, "Sub_D");
      checkSubButton(subE, lastSubButtonStates[5], p.t2_e, "Sub_E"); // Thêm nút E
      checkSubButton(subF, lastSubButtonStates[6], p.t2_f, "Sub_F"); // Thêm nút F (gán Num_3 trong Profile 2)
      checkSubButton(subK, lastSubButtonStates[4], p.t2_k, "Sub_K"); 
    }
  }
}

// =========================================================
// CÁC HÀM XỬ LÝ CON
// =========================================================

void triggerLedBlink(int flashes) {
  ledFlashesCount = flashes;
  lastLedToggle = millis();
  ledState = HIGH;
  digitalWrite(LED_PIN, ledState);
}

void checkButton(int pin, uint8_t key) {
  if (key == 0) return; 
  int currentState = digitalRead(pin);
  
  if (!isButtonInitialized) {
    for (int i = 0; i < 40; i++) lastButtonStates[i] = digitalRead(i);
    isButtonInitialized = true;
    currentState = digitalRead(pin); 
  }
  
  if (currentState != lastButtonStates[pin]) {
    if (currentState == PRESSED) {
      bleKeyboard.press(key);
      Serial.print("-> BUTTON [Pin "); Serial.print(pin); 
      Serial.print("] PRESSED. Sent key: '"); Serial.print((char)key); Serial.println("'");
    } else {
      bleKeyboard.release(key);
      Serial.print("<- BUTTON [Pin "); Serial.print(pin); 
      Serial.print("] RELEASED. Released key: '"); Serial.print((char)key); Serial.println("'");
    }
    lastButtonStates[pin] = currentState; 
  }
}

void checkSubButton(int currentVirtualState, int &lastVirtualState, uint8_t key, const char* btnName) {
  if (key == 0) return;
  
  if (currentVirtualState != lastVirtualState) {
    if (currentVirtualState == PRESSED) {
      bleKeyboard.press(key);
      Serial.print("-> TAY 2 ["); Serial.print(btnName); Serial.print("] PRESSED. Sent key: '"); Serial.print((char)key); Serial.println("'");
    } else {
      delay(30); // Giữ phím đủ lâu giúp game nhận diện ổn định
      bleKeyboard.release(key);
      Serial.print("<- TAY 2 ["); Serial.print(btnName); Serial.print("] RELEASED. Released key: '"); Serial.print((char)key); Serial.println("'");
    }
    lastVirtualState = currentVirtualState;
  }
}

void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown, int joyIndex, int threshLow, int threshHigh) {  
  if (keyLeft != 0) {
    if (xVal < threshLow && !isLeftPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyLeft); 
      isLeftPressedGlobal[joyIndex] = true; 
      lastLeftKeyGlobal[joyIndex] = keyLeft;
    } 
    else if (xVal >= threshLow && isLeftPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastLeftKeyGlobal[joyIndex]); 
      isLeftPressedGlobal[joyIndex] = false;
    }
  }
  
  if (keyRight != 0) {
    if (xVal > threshHigh && !isRightPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyRight); 
      isRightPressedGlobal[joyIndex] = true; 
      lastRightKeyGlobal[joyIndex] = keyRight;
    } 
    else if (xVal <= threshHigh && isRightPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastRightKeyGlobal[joyIndex]); 
      isRightPressedGlobal[joyIndex] = false;
    }
  }
  
  if (keyUp != 0) {
    if (yVal > JOY_THRESHOLD_HIGH && !isUpPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyUp); 
      isUpPressedGlobal[joyIndex] = true; 
      lastUpKeyGlobal[joyIndex] = keyUp;
    } 
    else if (yVal <= JOY_THRESHOLD_HIGH && isUpPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastUpKeyGlobal[joyIndex]); 
      isUpPressedGlobal[joyIndex] = false;
    }
  }
  
  if (keyDown != 0) {
    if (yVal < JOY_THRESHOLD_LOW && !isDownPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyDown); 
      isDownPressedGlobal[joyIndex] = true; 
      lastDownKeyGlobal[joyIndex] = keyDown;
    } 
    else if (yVal >= JOY_THRESHOLD_LOW && isDownPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastDownKeyGlobal[joyIndex]); 
      isDownPressedGlobal[joyIndex] = false;
    }
  }
}