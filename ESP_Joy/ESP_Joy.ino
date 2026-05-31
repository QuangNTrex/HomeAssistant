#include <BleKeyboard.h>

BleKeyboard bleKeyboard("Joy Gamer", "DIY Creator", 100);

const int LED_PIN = 2; 

const int PRESSED  = LOW;   
const int RELEASED = HIGH;  

// TAY CẦM 1
const int JOY1_X = 36; const int JOY1_Y = 39; const int JOY1_K = 12;
const int JOY1_A = 32; const int JOY1_B = 33; const int JOY1_C = 25;
const int JOY1_D = 26; const int JOY1_E = 27; const int JOY1_F = 13; // NÚT CHUYỂN PROFILE

// TAY CẦM 2
const int JOY2_X = 34; const int JOY2_Y = 35;
const int JOY2_A = 23; const int JOY2_B = 22; const int JOY2_C = 21; const int JOY2_D = 19;

const int JOY_THRESHOLD_LOW  = 600; 
const int JOY_THRESHOLD_HIGH = 3400; 

// CẤU TRÚC ĐƯỢC CHUẨN HÓA (Phân tách rõ ràng cụm di chuyển và cụm nút hành động)
struct GameProfile {
  // Hướng di chuyển (Joystick)
  uint8_t t1_left; uint8_t t1_right; uint8_t t1_up; uint8_t t1_down;
  uint8_t t2_left; uint8_t t2_right; uint8_t t2_up; uint8_t t2_down;

  // Các nút bấm hành động tương ứng từng chân phần cứng
  uint8_t t1_k; uint8_t t1_a; uint8_t t1_b; uint8_t t1_c; uint8_t t1_d; uint8_t t1_e;
  uint8_t t2_k; uint8_t t2_a; uint8_t t2_b; uint8_t t2_c; uint8_t t2_d;
};

// =========================================================
// KHỞI TẠO CÁC PROFILE ĐÃ ĐƯỢC SỬA LỖI ĐỦ PHẦN TỬ
// =========================================================

// Profile 0: pr ađa
GameProfile profile0 = { 
  // Di chuyển: Tay 1 | Tay 2
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  // Nút bấm: Tay 1 (K, A, B, C, D, E) | Tay 2 (K, A, B, C, D) -- Chân nào không dùng để 0
  0, 'w', 'd', 's', 'a', 0,
  0, KEY_UP_ARROW, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW
};

// Profile 1: bad ice
GameProfile profile1 = { 
  // Di chuyển: Tay 1 | Tay 2
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  // Nút bấm: Tay 1 (K, A, B, C, D, E) | Tay 2 (K, A, B, C, D)
  0, 'q', 0, 0, 0, 'r',
  0, ' ', 0, 0, 0
};

// Profile 2: Songoku 2.5 (Đã sửa chuẩn hóa theo đúng bộ nút game gốc)
GameProfile profile2 = { 
  // Di chuyển: Tay 1 | Tay 2
  'a', 'd', 'w', 's',  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW,
  // Nút hành động Tay 1 map vào: K=L(gồng), A=K(nhảy), B=O(biến hình), C=I(chưởng lớn), D=J(đấm), E=U(chưởng nhỏ)
  'l', 'k', 'o', 'i', 'j', 'u',
  // Nút hành động Tay 2 map vào: K=3(gồng), A=2(nhảy), B=6(biến hình), C=5(chưởng lớn), D=1(đấm) -> nếu thiếu nút chưởng nhỏ '4' có thể bỏ qua tùy phần cứng
  '3', '2', '6', '5', '1' 
};

GameProfile activeProfiles[] = {profile0, profile1, profile2};
int currentProfileIndex = 0; 
const int TOTAL_PROFILES = sizeof(activeProfiles) / sizeof(activeProfiles[0]);

unsigned long lastScanTime = 0;     
unsigned long lastLedToggle = 0;    
int ledFlashesCount = 0;            
int ledState = LOW;

int lastButtonStates[40]; 
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
void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown, int joyIndex);

void setup() {
  Serial.begin(115200); 
  bleKeyboard.begin();
  pinMode(LED_PIN, OUTPUT);
  
  // Thêm JOY1_K và loại bỏ các chân thừa để tối ưu bộ nhớ cấu hình chân
  int pins[] = {JOY1_K, JOY1_A, JOY1_B, JOY1_C, JOY1_D, JOY1_E, JOY1_F, JOY2_A, JOY2_B, JOY2_C, JOY2_D};
  for (int pin : pins) pinMode(pin, INPUT_PULLUP);
  
  triggerLedBlink(1); 
}

void loop() {
  unsigned long currentTime = millis();

  // 1. LOGIC CHUYỂN PROFILE
  int currentButtonFState = digitalRead(JOY1_F);
  if (lastButtonFState == RELEASED && currentButtonFState == PRESSED) {
    currentProfileIndex = (currentProfileIndex + 1) % TOTAL_PROFILES;
    bleKeyboard.releaseAll(); 
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

  // 2. QUÈT VÀ TRUYỀN PHÍM CHU KỲ 10MS
  if (currentTime - lastScanTime >= 10) {
    lastScanTime = currentTime;

    if (bleKeyboard.isConnected()) {
      GameProfile p = activeProfiles[currentProfileIndex]; 

      // QUÉT TAY CẦM 1 (Đã bổ sung nút JOY1_K)
      checkJoystick(analogRead(JOY1_X), analogRead(JOY1_Y), p.t1_left, p.t1_right, p.t1_up, p.t1_down, 0);
      checkButton(JOY1_K, p.t1_k); 
      checkButton(JOY1_A, p.t1_a); checkButton(JOY1_B, p.t1_b); checkButton(JOY1_C, p.t1_c);
      checkButton(JOY1_D, p.t1_d); checkButton(JOY1_E, p.t1_e); 

      // QUÉT TAY CẦM 2 (Đã bổ sung nút gạt tiềm năng ảo nếu có chân cơ lý / ở đây giữ nguyên số chân quét của bạn)
      checkJoystick(analogRead(JOY2_X), analogRead(JOY2_Y), p.t2_left, p.t2_right, p.t2_up, p.t2_down, 1);
      checkButton(JOY2_A, p.t2_a); checkButton(JOY2_B, p.t2_b); checkButton(JOY2_C, p.t2_c); checkButton(JOY2_D, p.t2_d);
    }
  }
}

// Giữ nguyên các hàm checkButton, checkJoystick và triggerLedBlink phía dưới của bạn...

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
  
  // Khởi tạo trạng thái thật cho toàn bộ các chân ở vòng quét đầu tiên
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

void checkJoystick(int xVal, int yVal, uint8_t keyLeft, uint8_t keyRight, uint8_t keyUp, uint8_t keyDown, int joyIndex) {  
  // --- TRỤC X: TRÁI / PHẢI ---
  if (keyLeft != 0) {
    if (xVal < JOY_THRESHOLD_LOW && !isLeftPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyLeft); 
      isLeftPressedGlobal[joyIndex] = true; 
      lastLeftKeyGlobal[joyIndex] = keyLeft;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt TRÁI. Sent key: '"); Serial.print((char)keyLeft); Serial.println("'");
    } 
    else if (xVal >= JOY_THRESHOLD_LOW && isLeftPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastLeftKeyGlobal[joyIndex]); 
      isLeftPressedGlobal[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả TRÁI. Released key: '"); Serial.print((char)lastLeftKeyGlobal[joyIndex]); Serial.println("'");
    }
  }
  
  if (keyRight != 0) {
    if (xVal > JOY_THRESHOLD_HIGH && !isRightPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyRight); 
      isRightPressedGlobal[joyIndex] = true; 
      lastRightKeyGlobal[joyIndex] = keyRight;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt PHẢI. Sent key: '"); Serial.print((char)keyRight); Serial.println("'");
    } 
    else if (xVal <= JOY_THRESHOLD_HIGH && isRightPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastRightKeyGlobal[joyIndex]); 
      isRightPressedGlobal[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả PHẢI. Released key: '"); Serial.print((char)lastRightKeyGlobal[joyIndex]); Serial.println("'");
    }
  }
  
  // =========================================================
  // --- TRỤC Y: LÊN / XUỐNG (ĐÃ ĐẢO LOGIC THEO PHẦN CỨNG THỰC TẾ) ---
  // =========================================================
  
  // 1. XỬ LÝ HƯỚNG LÊN (Khi kéo lên, giá trị đạt tối đa -> Vượt ngưỡng HIGH)
  if (keyUp != 0) {
    if (yVal > JOY_THRESHOLD_HIGH && !isUpPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyUp); 
      isUpPressedGlobal[joyIndex] = true; 
      lastUpKeyGlobal[joyIndex] = keyUp;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt LÊN. Sent key: '"); Serial.print((char)keyUp); Serial.println("'");
    } 
    else if (yVal <= JOY_THRESHOLD_HIGH && isUpPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastUpKeyGlobal[joyIndex]); 
      isUpPressedGlobal[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả LÊN. Released key: '"); Serial.print((char)lastUpKeyGlobal[joyIndex]); Serial.println("'");
    }
  }
  
  // 2. XỬ LÝ HƯỚNG XUỐNG (Khi gạt xuống, giá trị tụt về 0 -> Thấp hơn ngưỡng LOW)
  if (keyDown != 0) {
    if (yVal < JOY_THRESHOLD_LOW && !isDownPressedGlobal[joyIndex]) {
      bleKeyboard.press(keyDown); 
      isDownPressedGlobal[joyIndex] = true; 
      lastDownKeyGlobal[joyIndex] = keyDown;
      Serial.print("-> TAY "); Serial.print(joyIndex + 1); Serial.print(" gạt XUỐNG. Sent key: '"); Serial.print((char)keyDown); Serial.println("'");
    } 
    else if (yVal >= JOY_THRESHOLD_LOW && isDownPressedGlobal[joyIndex]) {
      bleKeyboard.release(lastDownKeyGlobal[joyIndex]); 
      isDownPressedGlobal[joyIndex] = false;
      Serial.print("<- TAY "); Serial.print(joyIndex + 1); Serial.print(" thả XUỐNG. Released key: '"); Serial.print((char)lastDownKeyGlobal[joyIndex]); Serial.println("'");
    }
  }
}