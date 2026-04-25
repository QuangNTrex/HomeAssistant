#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

hd44780_I2Cexp lcd;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Start");

  Wire.begin(D1, D2);

  int status = lcd.begin(16, 2);
  Serial.print("LCD status = ");
  Serial.println(status);

  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("HELLO ESP8266");
}

void loop() {}