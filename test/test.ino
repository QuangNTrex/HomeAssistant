// Äá»‹nh nghÄ©a chÃ¢n theo sÆ¡ Ä‘á»“ má»›i cá»§a báº¡n
const int motorM1 = 15; // ChÃ¢n D8 Ä‘iá»u khiá»ƒn Äá»™ng cÆ¡ (M1)
const int lightM2 = 12; // ChÃ¢n D6 Ä‘iá»u khiá»ƒn BÃ³ng Ä‘Ã¨n (M2)

void setup() {
  // Cáº¥u hÃ¬nh chÃ¢n lÃ  Ä‘áº§u ra
  pinMode(motorM1, OUTPUT);
  pinMode(lightM2, OUTPUT);
  
  // Äáº£m báº£o má»i thá»© táº¯t khi vá»«a khá»Ÿi Ä‘á»™ng
  digitalWrite(motorM1, LOW);
  digitalWrite(lightM2, LOW);

  Serial.begin(115200);
  Serial.println("--- Bat dau Test Motor (D8) va Den (D6) ---");
}

void loop() {
  // Ká»ŠCH Báº¢N 1: Báº­t Ä‘Ã¨n vÃ  cháº¡y motor tá»‘c Ä‘á»™ trung bÃ¬nh
  Serial.println("Den: BAT | Motor: 50%");
  digitalWrite(lightM2, HIGH);       // Báº­t Ä‘Ã¨n
  analogWrite(motorM1, 512);        // Cháº¡y motor (ESP8266 PWM: 0-1023)
  delay(4000);

  // Ká»ŠCH Báº¢N 2: Táº¯t Ä‘Ã¨n vÃ  tÄƒng tá»‘c motor tá»‘i Ä‘a
  Serial.println("Den: TAT | Motor: 100%");
  digitalWrite(lightM2, LOW);        // Táº¯t Ä‘Ã¨n
  analogWrite(motorM1, 1023);       // Motor cháº¡y cá»±c Ä‘áº¡i
  delay(4000);

  // Ká»ŠCH Báº¢N 3: Dá»«ng táº¥t cáº£
  Serial.println("Dung tat ca.");
  analogWrite(motorM1, 0);          // Dá»«ng motor
  digitalWrite(lightM2, LOW);       // Táº¯t Ä‘Ã¨n
  delay(2000);
}
