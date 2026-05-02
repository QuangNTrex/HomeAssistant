#define TRIG_PIN D3   // GPIO0
#define ECHO_PIN D8   // GPIO15

long duration;
float distance;

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.begin(115200);
  delay(1000);
  Serial.println("HC-SR04 Ready");
}

void loop() {
  // đảm bảo TRIG LOW
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // phát xung 10us
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // đọc ECHO
  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // tính khoảng cách
  distance = duration * 0.034 / 2;

  // kiểm tra lỗi
  if (duration == 0) {
    Serial.println("Out of range");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  delay(500);
}