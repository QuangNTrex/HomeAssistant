#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(D1, D2);

  Serial.println("I2C Scanner");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if(error == 0) {
      Serial.print("Found I2C at 0x");
      Serial.println(address, HEX);
      nDevices++;
    }
  }

  if(nDevices == 0) Serial.println("No I2C devices found");

  delay(3000);
}