/*
   MẠCH TEST PHẦN CỨNG 2 TAY CẦM CHƠI GAME (ESP32 38-PIN)
   Cấu hình theo sơ đồ nút bấm Active Low (INPUT_PULLUP)
*/

// ==========================================
// ĐỊNH NGHĨA CHÂN TAY CẦM 1 (HÀNG BÊN TRÁI)
// ==========================================
const int JOY1_X = 36; // Chân Analog đọc Trục X
const int JOY1_Y = 39; // Chân Analog đọc Trục Y
const int JOY1_A = 32; // Chân Digital đọc Nút A
const int JOY1_B = 33; // Chân Digital đọc Nút B
const int JOY1_C = 25; // Chân Digital đọc Nút C
const int JOY1_D = 26; // Chân Digital đọc Nút D
const int JOY1_E = 27; // Chân Digital đọc Nút E
const int JOY1_F = 13; // Chân Digital đọc Nút F

// ==========================================
// ĐỊNH NGHĨA CHÂN TAY CẦM 2 (HÀNG BÊN PHẢI + ANALOG)
// ==========================================
const int JOY2_X = 34; // Chân Analog đọc Trục X
const int JOY2_Y = 35; // Chân Analog đọc Trục Y
const int JOY2_A = 23; // Chân Digital đọc Nút A
const int JOY2_B = 22; // Chân Digital đọc Nút B
const int JOY2_C = 21; // Chân Digital đọc Nút C
const int JOY2_D = 19; // Chân Digital đọc Nút D

void setup() {
  // Khởi tạo cổng Serial giao tiếp với máy tính
  Serial.begin(115200);

  // Cấu hình các chân Digital của TAY CẦM 1 là INPUT_PULLUP
  pinMode(JOY1_A, INPUT_PULLUP);
  pinMode(JOY1_B, INPUT_PULLUP);
  pinMode(JOY1_C, INPUT_PULLUP);
  pinMode(JOY1_D, INPUT_PULLUP);
  pinMode(JOY1_E, INPUT_PULLUP);
  pinMode(JOY1_F, INPUT_PULLUP);

  // Cấu hình các chân Digital của TAY CẦM 2 là INPUT_PULLUP
  pinMode(JOY2_A, INPUT_PULLUP);
  pinMode(JOY2_B, INPUT_PULLUP);
  pinMode(JOY2_C, INPUT_PULLUP);
  pinMode(JOY2_D, INPUT_PULLUP);

  Serial.println("\n=============================================");
  Serial.println("HE THONG CHUAN BI KIẺM TRA TAY CAM... OK!");
  Serial.println("=============================================");
}

void loop() {
  // 1. Đọc giá trị Analog của Joystick (Giá trị từ 0 đến 4095)
  int j1_x_val = analogRead(JOY1_X);
  int j1_y_val = analogRead(JOY1_Y);
  int j2_x_val = analogRead(JOY2_X);
  int j2_y_val = analogRead(JOY2_Y);

  // 2. Đọc trạng thái các nút bấm Digital 
  // Vì dùng INPUT_PULLUP, nút bấm khi nhấn sẽ trả về giá trị 0 (LOW).
  // Thêm dấu chấm than (!) ở trước để đảo ngược lại: 1 là ĐANG BẤM, 0 là THẢ RA.
  bool btn1_a = !digitalRead(JOY1_A);
  bool btn1_b = !digitalRead(JOY1_B);
  bool btn1_c = !digitalRead(JOY1_C);
  bool btn1_d = !digitalRead(JOY1_D);
  bool btn1_e = !digitalRead(JOY1_E);
  bool btn1_f = !digitalRead(JOY1_F);

  bool btn2_a = !digitalRead(JOY2_A);
  bool btn2_b = !digitalRead(JOY2_B);
  bool btn2_c = !digitalRead(JOY2_C);
  bool btn2_d = !digitalRead(JOY2_D);

  // 3. IN KẾT QUẢ CỦA TAY CẦM 1 RA SERIAL MONITOR
  Serial.print("TAY 1 -> X: "); Serial.print(j1_x_val);
  Serial.print(" | Y: "); Serial.print(j1_y_val);
  Serial.print(" | Nut: [");
  if (btn1_a) Serial.print(" A ");
  if (btn1_b) Serial.print(" B ");
  if (btn1_c) Serial.print(" C ");
  if (btn1_d) Serial.print(" D ");
  if (btn1_e) Serial.print(" E ");
  if (btn1_f) Serial.print(" F ");
  Serial.print("]");

  // Khoảng cách phân tách giữa 2 tay cầm
  Serial.print("   |||   ");

  // 4. IN KẾT QUẢ CỦA TAY CẦM 2 RA SERIAL MONITOR
  Serial.print("TAY 2 -> X: "); Serial.print(j2_x_val);
  Serial.print(" | Y: "); Serial.print(j2_y_val);
  Serial.print(" | Nut: [");
  if (btn2_a) Serial.print(" A ");
  if (btn2_b) Serial.print(" B ");
  if (btn2_c) Serial.print(" C ");
  if (btn2_d) Serial.print(" D ");
  Serial.println("]");

  // Chờ 250ms (0.25 giây) rồi quét tiếp để tránh tràn màn hình Monitor
  delay(250);
}