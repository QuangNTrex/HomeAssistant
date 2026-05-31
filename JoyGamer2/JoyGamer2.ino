// Code dành cho ESP8266 - Cấu hình chân D theo yêu cầu mới
const int PIN_X = A0;  // Trục X của Joystick

// Định nghĩa lại các chân nút bấm theo đúng cấu hình của bạn
const int PIN_A = D0;
const int PIN_B = D1;
const int PIN_C = D2;
const int PIN_D = D4;  // CHÚ Ý: Chân Boot, không giữ nút D khi cắm nguồn/Reset
const int PIN_E = D5;
const int PIN_F = D6;
const int PIN_K = D7;  // Nút nhấn tích hợp trên cần Joystick (SW)

void setup() {
  // Khởi tạo cổng Serial gửi dữ liệu sang ESP32 (Tốc độ 115200)
  Serial.begin(115200);

  // Cấu hình các chân có hỗ trợ Pullup nội bộ
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, INPUT_PULLUP);
  pinMode(PIN_E, INPUT_PULLUP);
  pinMode(PIN_F, INPUT_PULLUP);
  pinMode(PIN_K, INPUT_PULLUP);

  // Riêng chân D0 (Nút A) không có Pullup nội bộ trên ESP8266.
  // Nếu mạch Joystick Shield của bạn ĐÃ CÓ SẴN trở kéo lên (hầu hết các shield đều có), bạn để INPUT.
  // Nếu bạn nối nút bấm rời trực tiếp từ chân D0 xuống G (GND), bạn nên hàn thêm 1 con điện trở 10k Ohm nối từ D0 lên nguồn 3.3V.
  pinMode(PIN_A, INPUT_PULLUP); 
}

void loop() {
  // Đọc giá trị của Trục X (0 đến 1023)
  int valX = analogRead(PIN_X);

  // Đọc trạng thái của 7 nút bấm (Chưa bấm = 1, Nhấn nút = 0)
  int btnA = digitalRead(PIN_A);
  int btnB = digitalRead(PIN_B);
  int btnC = digitalRead(PIN_C);
  int btnD = digitalRead(PIN_D);
  int btnE = digitalRead(PIN_E);
  int btnF = digitalRead(PIN_F);
  int btnK = digitalRead(PIN_K);

  // Đóng gói chuỗi dữ liệu định dạng mới: TrụcX,A,B,C,D,E,F,K
  // Ví dụ khi không bấm gì: 512,1,1,1,1,1,1,1
  Serial.print(valX); Serial.print(",");
  Serial.print(btnA); Serial.print(",");
  Serial.print(btnB); Serial.print(",");
  Serial.print(btnC); Serial.print(",");
  Serial.print(btnD); Serial.print(",");
  Serial.print(btnE); Serial.print(",");
  Serial.print(btnF); Serial.print(",");
  Serial.println(btnK); // Ký tự \n xuống dòng để báo kết thúc gói dữ liệu

  delay(20); // Gửi liên tục mỗi 20ms để đảm bảo độ nhạy cao khi chơi game
}