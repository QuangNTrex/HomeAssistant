#include <BleKeyboard.h>

// Khởi tạo bàn phím Bluetooth
BleKeyboard bleKeyboard("Joy Gamer", "DIY Creator", 100);

// Định nghĩa chân LED báo hiệu hệ thống trên ESP32
const int LED_PIN = 2; 

// =========================================================
// ĐỊNH NGHĨA TRẠNG THÁI LOGIC NÚT BẤM (ACTIVE LOW - CLEAN CODE)
// =========================================================
const int PRESSED  = LOW;   // Khi bấm nút, chân mạch bị kéo xuống GND (Mức THẤP)
const int RELEASED = HIGH;  // Khi thả nút, điện trở treo kéo chân lên 3.3V (Mức CAO)

// =========================================================
// CẤU HÌNH ĐỊNH NGHĨA CHÂN PHẦN CỨNG (MAP CHÂN CHUẨN CỦA BẠN)
// =========================================================
// TAY CẦM 1 (HÀNG BÊN TRÁI ESP32)
const int JOY1_X = 36; const int JOY1_Y = 39; const int JOY1_K = 12;
const int JOY1_A = 32; const int JOY1_B = 33; const int JOY1_C = 25;
const int JOY1_D = 26; const int JOY1_E = 27; const int JOY1_F = 13; // NÚT F DÙNG CHUYỂN PROFILE

// TAY CẦM 2 (HÀNG BÊN PHẢI ESP32 + 2 CHÂN ANALOG TRÊN HÀNG TRÁI)
const int JOY2_X = 34; const int JOY2_Y = 35;
const int JOY2_A = 23; const int JOY2_B = 22; const int JOY2_C = 21; const int JOY2_D = 19;

// Ngưỡng kích hoạt tín hiệu của cần gạt Joystick (Dải Analog từ 0 - 4095)
const int JOY_THRESHOLD_LOW  = 1000; // Gạt kịch sang Trái / Lên (Áp tiệm cận về 0V)
const int JOY_THRESHOLD_HIGH = 3000; // Gạt kịch sang Phải / Xuống (Áp tiệm cận về 3.3V)

// =========================================================
// CẤU TRÚC ĐỐI TƯỢNG BỘ QUY TẮC PHÍM (GAME PROFILE)
// =========================================================
struct GameProfile {
  // Bản đồ phím cho TAY CẦM 1 (Cần gạt + Nút bấm)
  uint8_t t1_left; uint8_t t1_right; uint8_t t1_up; uint8_t t1_down;
  uint8_t t1_a;    uint8_t t1_b;     uint8_t t1_c;  uint8_t t1_d; uint8_t t1_e; uint8_t t1_f;

  // Bản đồ phím cho TAY CẦM 2 (Cần gạt + Nút bấm)
  uint8_t t2_left; uint8_t t2_right; uint8_t t2_up; uint8_t t2_down;
  uint8_t t2_a;    uint8_t t2_b;     uint8_t t2_c;  uint8_t t2_d;
};

// Khởi tạo các Đối tượng bắt đầu từ profile0
// Lưu ý: t1_f của các profile luôn để bằng 0 vì nút F dùng làm nút hệ thống chuyển chế độ
GameProfile profile0 = { 
  'a', 'd', 'w', 's', 'a', 'w', 'd', 0, 0, 0, 
  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW, 0, 0, 0, 0 
};

GameProfile profile1 = { 
  'a', 'd', 'w', 's', 0, 0, 0, ' ', 'q', 0, 
  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW, 0, 0, 0, 'm' 
};

GameProfile profile2 = { 
  'a', 'd', 'w', 's', 0, 0, 0, 'j', 'k', 0, 
  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW, 0, 0, 0, '1' 
};

// Mảng quản lý các đối tượng Profile
GameProfile activeProfiles[] = {profile0, profile1, profile2};
int currentProfileIndex = 0; // Bắt đầu từ vị trí 0 (profile0)
const int TOTAL_PROFILES = 3;

// Các biến quản lý thời gian và trạng thái ngầm (Non-blocking)
unsigned long lastScanTime = 0;     
unsigned long lastLedToggle = 0;    
int ledFlashesCount = 0;            
int ledState = LOW;

// Biến bắt cạnh nút F phục vụ logic Single Tap công nghệ cao
int lastButtonFState = RELEASED; 

void setup() {
  Serial.begin(115200);
  bleKeyboard.begin();
  pinMode(LED_PIN, OUTPUT);
  
  // Khởi tạo INPUT_PULLUP cho toàn bộ nút bấm bằng mảng tự động
  int pins[] = {JOY1_K, JOY1_A, JOY1_B, JOY1_C, JOY1_D, JOY1_E, JOY1_F, JOY2_A, JOY2_B, JOY2_C, JOY2_D};
  for (int pin : pins) pinMode(pin, INPUT_PULLUP);
  
  // Bật nguồn: Nháy LED 1 lần báo hiệu đang sẵn sàng ở profile0
  triggerLedBlink(1); 
}

void loop() {
  unsigned long currentTime = millis();

  // ---------------------------------------------------------
  // 1. LOGIC CHUYỂN PROFILE: SINGLE TAP NÚT F (BẮT CẠNH XUỐNG)
  // ---------------------------------------------------------
  int currentButtonFState = digitalRead(JOY1_F);
  
  // Nếu trạng thái trước đó là THẢ (RELEASED) và hiện tại là BẤM (PRESSED)
  if (lastButtonFState == RELEASED && currentButtonFState == PRESSED) {
    // Xoay vòng chỉ số mảng: 0 -> 1 -> 2 -> 0
    currentProfileIndex = (currentProfileIndex + 1) % TOTAL_PROFILES;
    
    // Thả nhanh toàn bộ các nút bấm cũ để tránh kẹt phím sang game mới
    bleKeyboard.releaseAll(); 
    
    // Nháy LED tương ứng: profile0 nháy 1, profile1 nháy 2, profile2 nháy 3
    triggerLedBlink(currentProfileIndex + 1); 
  }
  lastButtonFState = currentButtonFState; // Lưu trạng thái hiện tại làm trạng thái cũ cho vòng quét sau

  // LOGIC NHẤP NHÁY ĐÈN LED NGẦM (Chạy song song, không block CPU)
  if (ledFlashesCount > 0) {
    if (currentTime - lastLedToggle >= 150) { 
      lastLedToggle = currentTime;
      ledState = (ledState == LOW) ? HIGH : LOW;
      digitalWrite(LED_PIN, ledState);
      if (ledState == LOW) ledFlashesCount--; 
    }
  }

  // ---------------------------------------------------------
  // 2. THỰC THI QUÉT VÀ TRUYỀN PHÍM GAME CHU KỲ 10MS
  // ---------------------------------------------------------
  if (currentTime - lastScanTime >= 10) {
    lastScanTime = currentTime;

    if (bleKeyboard.isConnected()) {
      GameProfile p = activeProfiles[currentProfileIndex]; // Lấy đối tượng bộ quy tắc hiện tại

      // Xử lý TAY CẦM 1 (Bỏ qua nút F vì dành riêng cho hệ thống)
      checkJoystick(analogRead(JOY1_X), analogRead(JOY1_Y), p.t1_left, p.t1_right, p.t1_up, p.t1_down);
      checkButton(JOY1_A, p.t1_a); checkButton(JOY1_B, p.t1_b); checkButton(JOY1_C, p.t1_c);
      checkButton(JOY1_D, p.t1_d); checkButton(JOY1_E, p.t1_e); 

      // Xử lý TAY CẦM 2
      checkJoystick(analogRead(JOY2_X), analogRead(JOY2_Y), p.t2_left, p.t2_right, p.t2_up, p.t2_down);
      checkButton(JOY2_A, p.t2_a); checkButton(JOY2_B, p.t2_b); checkButton(JOY2_C, p.t2_c); checkButton(JOY2_D, p.t2_d);
    }
  }
}

// =========================================================
// CÁC HÀM XỬ LÝ CON ĐÃ ĐƯỢC CHUYỂN ĐỔI SANG HẰNG SỐ CHUẨN
// =========================================================

// Kích hoạt biến đếm chớp LED ngầm
void triggerLedBlink(int flashes) {
  ledFlashesCount = flashes;
  lastLedToggle = millis();
  ledState = HIGH;
  digitalWrite(LED_PIN, ledState);
}

// Kiểm tra và nhấn phím Digital tường minh
void checkButton(int pin, uint8_t key) {
  if (key == 0) return; // Nếu phím trong profile bằng 0 tức là không sử dụng
  
  if (digitalRead(pin) == PRESSED) {
    bleKeyboard.press(key); 
  } else { 
    bleKeyboard.release(key); 
  }
}

// Kiểm tra cần gạt Analog và ép phím Digital
void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown) {
  
  // --- KIỂM TRA TRỤC X (Trái / Phải) ---
  // Nếu giá trị Analog tụt thấp hơn Ngưỡng Dưới -> Kích hoạt phím Sang Trái
  if (xVal < JOY_THRESHOLD_LOW && keyLeft != 0) {
    bleKeyboard.press(keyLeft); 
  } else if (keyLeft != 0) {
    bleKeyboard.release(keyLeft);
  }
  
  // Nếu giá trị Analog vượt cao hơn Ngưỡng Trên -> Kích hoạt phím Sang Phải
  if (xVal > JOY_THRESHOLD_HIGH && keyRight != 0) {
    bleKeyboard.press(keyRight); 
  } else if (keyRight != 0) {
    bleKeyboard.release(keyRight);
  }
  
  // --- KIỂM TRA TRỤC Y (Lên / Xuống) ---
  // Nếu giá trị Analog tụt thấp hơn Ngưỡng Dưới -> Kích hoạt phím Lên (Nhảy)
  if (yVal < JOY_THRESHOLD_LOW && keyUp != 0) {
    bleKeyboard.press(keyUp); 
  } else if (keyUp != 0) {
    bleKeyboard.release(keyUp);
  }
  
  // Nếu giá trị Analog vượt cao hơn Ngưỡng Trên -> Kích hoạt phím Xuống (Cúi)
  if (yVal > JOY_THRESHOLD_HIGH && keyDown != 0) {
    bleKeyboard.press(keyDown); 
  } else if (keyDown != 0) {
    bleKeyboard.release(keyDown);
  }
}

void checkButton(int pin, uint8_t key) {
  if (key == 0) return; 
  
  int currentState = digitalRead(pin);
  
  // SỬA LỖI: Sử dụng một biến cờ để kiểm tra xem mảng đã được khởi tạo theo thực tế chưa
  static bool isInitialized = false;
  static int lastStates[40]; 
  
  if (!isInitialized) {
    // Vòng quét đầu tiên khi cắm nguồn: Đọc trạng thái thật của toàn bộ các chân
    for (int i = 0; i < 40; i++) lastStates[i] = digitalRead(i);
    isInitialized = true;
    currentState = digitalRead(pin); // Cập nhật lại giá trị hiện tại
  }
  
  if (currentState != lastStates[pin]) {
    if (currentState == PRESSED) {
      bleKeyboard.press(key);
      Serial.print("-> BUTTON [Pin "); Serial.print(pin); 
      Serial.print("] PRESSED. Sent key: '"); Serial.print((char)key); Serial.println("'");
    } else {
      bleKeyboard.release(key);
      Serial.print("<- BUTTON [Pin "); Serial.print(pin); 
      Serial.print("] RELEASED. Released key: '"); Serial.print((char)key); Serial.println("'");
    }
    lastStates[pin] = currentState; 
  }
}

// Thêm tham số 'joyIndex' (0 cho Tay 1, 1 cho Tay 2) để phân biệt ô nhớ
void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown, int joyIndex) {
  
  // Tạo mảng static 2 phần tử để lưu biệt lập trạng thái của Tay 1 (index 0) và Tay 2 (index 1)
  static bool isLeftPressed[2]  = {false, false};
  static bool isRightPressed[2] = {false, false};
  static bool isUpPressed[2]    = {false, false};
  static bool isDownPressed[2]  = {false, false};
  
  static uint8_t lastLeftKey[2]  = {0, 0};
  static uint8_t lastRightKey[2] = {0, 0};
  static uint8_t lastUpKey[2]    = {0, 0};
  static uint8_t lastDownKey[2]  = {0, 0};

  // --- TRỤC X: TRÁI / PHẢI ---
  if (keyLeft != 0) {
    if (xVal < JOY_THRESHOLD_LOW && !isLeftPressed[joyIndex]) {
      bleKeyboard.press(keyLeft); isLeftPressed[joyIndex] = true; lastLeftKey[joyIndex] = keyLeft;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt TRÁI. Sent key: '"); Serial.print((char)keyLeft); Serial.println("'");
    } else if (xVal >= JOY_THRESHOLD_LOW && isLeftPressed[joyIndex]) {
      bleKeyboard.release(lastLeftKey[joyIndex]); isLeftPressed[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả TRÁI. Released key: '"); Serial.print((char)lastLeftKey[joyIndex]); Serial.println("'");
    }
  }
  
  if (keyRight != 0) {
    if (xVal > JOY_THRESHOLD_HIGH && !isRightPressed[joyIndex]) {
      bleKeyboard.press(keyRight); isRightPressed[joyIndex] = true; lastRightKey[joyIndex] = keyRight;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt PHẢI. Sent key: '"); Serial.print((char)keyRight); Serial.println("'");
    } else if (xVal <= JOY_THRESHOLD_HIGH && isRightPressed[joyIndex]) {
      bleKeyboard.release(lastRightKey[joyIndex]); isRightPressed[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả PHẢI. Released key: '"); Serial.print((char)lastRightKey[joyIndex]); Serial.println("'");
    }
  }
  
  // --- TRỤC Y: LÊN / XUỐNG ---
  if (keyUp != 0) {
    if (yVal < JOY_THRESHOLD_LOW && !isUpPressed[joyIndex]) {
      bleKeyboard.press(keyUp); isUpPressed[joyIndex] = true; lastUpKey[joyIndex] = keyUp;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt LÊN. Sent key: '"); Serial.print((char)keyUp); Serial.println("'");
    } else if (yVal >= JOY_THRESHOLD_LOW && isUpPressed[joyIndex]) {
      bleKeyboard.release(lastUpKey[joyIndex]); isUpPressed[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả LÊN. Released key: '"); Serial.print((char)lastUpKey[joyIndex]); Serial.println("'");
    }
  }
  
  if (keyDown != 0) {
    if (yVal > JOY_THRESHOLD_HIGH && !isDownPressed[joyIndex]) {
      bleKeyboard.press(keyDown); isDownPressed[joyIndex] = true; lastDownKey[joyIndex] = keyDown;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt XUỐNG. Sent key: '"); Serial.print((char)keyDown); Serial.println("'");
    } else if (yVal <= JOY_THRESHOLD_HIGH && isDownPressed[joyIndex]) {
      bleKeyboard.release(lastDownKey[joyIndex]); isDownPressed[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả XUỐNG. Released key: '"); Serial.print((char)lastDownKey[joyIndex]); Serial.println("'");
    }
  }
}