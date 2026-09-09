/*
 * PROJECT VARUNA 1.0 — I2C Scanner Utility
 * Competition: Mini Electric Vehicle Competition 3.0 (Line Follower Race)
 * Hardware: ESP32 38-Pin Classic WROOM Board
 * 
 * Description:
 * Scans the I2C bus on GPIO21 (SDA) and GPIO22 (SCL) to verify the exact I2C addresses
 * of the two PCF8574 I/O expanders.
 * 
 * Target Addresses:
 * - PCF #1 (LEFT Sensor Module 1):  0x20 (or 0x38 if PCF8574A variant)
 * - PCF #2 (RIGHT Sensor Module 2): 0x24 (or 0x39 if PCF8574A variant)
 * 
 * Serial Monitor Baud Rate: 115200
 */

#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for serial connection
  
  Serial.println();
  Serial.println("==========================================");
  Serial.println("   PROJECT VARUNA 1.0 — I2C BUS SCANNER   ");
  Serial.println("==========================================");
  Serial.printf("Configuring I2C Bus: SDA = GPIO%d, SCL = GPIO%d\n", I2C_SDA, I2C_SCL);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // Standard 100kHz I2C clock
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("\n[SCANNING I2C BUS...]");
  
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf(" -> Found device at 7-bit address 0x%02X (%d)", address, address);
      
      // Identify PCF8574 / PCF8574A expected address ranges
      if (address == 0x20) {
        Serial.print(" <--- Target: PCF #1 (LEFT Sensor Board)");
      } else if (address == 0x24) {
        Serial.print(" <--- Target: PCF #2 (RIGHT Sensor Board)");
      } else if (address >= 0x20 && address <= 0x27) {
        Serial.print(" (Standard PCF8574 variant range)");
      } else if (address == 0x38) {
        Serial.print(" <--- PCF8574A Variant (LEFT Sensor Board)");
      } else if (address == 0x39) {
        Serial.print(" <--- PCF8574A Variant (RIGHT Sensor Board)");
      } else if (address >= 0x38 && address <= 0x3F) {
        Serial.print(" (PCF8574A variant range)");
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.printf(" -> Unknown error at address 0x%02X\n", address);
    }
  }

  if (nDevices == 0) {
    Serial.println("ERROR: No I2C devices found! Please check:");
    Serial.println(" 1. VCC and GND connections to both PCF8574 modules");
    Serial.println(" 2. SDA (GPIO21) and SCL (GPIO22) wiring");
    Serial.println(" 3. Pull-up resistors (PCF8574 modules usually have built-in pullups)");
  } else {
    Serial.printf("Scan complete. Total devices found: %d\n", nDevices);
  }

  Serial.println("------------------------------------------");
  Serial.println("Re-scanning in 3 seconds...");
  delay(3000);
}
