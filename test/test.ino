#define PIN1 D4
#define PIN2 D1
#define PIN3 D2

void setup() {
  pinMode(PIN1, OUTPUT);
  pinMode(PIN2, OUTPUT);
  pinMode(PIN3, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  // Bật cả 3 chân
  digitalWrite(PIN1, HIGH);
  digitalWrite(PIN2, HIGH);
  digitalWrite(PIN3, HIGH);
  Serial.println("All ON");

  delay(2000);

  // Tắt cả 3 chân
  digitalWrite(PIN1, LOW);
  digitalWrite(PIN2, LOW);
  digitalWrite(PIN3, LOW);
  Serial.println("All OFF");

  delay(2000);
}