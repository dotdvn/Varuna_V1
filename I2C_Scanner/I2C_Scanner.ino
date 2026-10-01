#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  Serial.println();
  Serial.println("Varuna I2C Address Scanner");
  Serial.printf("SDA: GPIO%u | SCL: GPIO%u\n", I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("Open Serial Monitor at 115200 baud.");
  Serial.println("Scanning addresses 0x03-0x77...\n");
}

void loop() {
  uint8_t found = 0;

  Serial.println("--- Scan ---");
  for (uint8_t address = 0x03; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Found device at 0x%02X", address);
      if (address >= 0x20 && address <= 0x27) {
        Serial.print("  (PCF8574)");
      } else if (address >= 0x38 && address <= 0x3F) {
        Serial.print("  (PCF8574A)");
      } else if (address == 0x48 || address == 0x49) {
        Serial.print("  (ADS1115 likely)");
      } else if (address == 0x68 || address == 0x69) {
        Serial.print("  (MPU6050 likely)");
      }
      Serial.println();
      ++found;
    }
    delay(2);
  }

  if (found == 0) {
    Serial.println("No I2C devices found.");
    Serial.println("Check 3.3 V, GND, SDA, SCL, and pull-up resistors.");
  } else {
    Serial.printf("%u device(s) found.\n", found);
    Serial.println("Use the detected PCF8574 address in Config.h.");
  }

  Serial.println("Scanning again in 3 seconds...\n");
  delay(3000);
}
