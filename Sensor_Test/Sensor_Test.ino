/*
 * PROJECT VARUNA 1.0 - Direct 7-Channel Digital IR Sensor Test
 * Sensor order LEFT -> RIGHT:
 * S1 GPIO13, S2 GPIO14, S3 GPIO25, S4 GPIO26,
 * S5 GPIO27, S6 GPIO33, S7 GPIO34
 *
 * Connect each sensor DO output to its GPIO and share GND.
 * Do not connect a 5V DO signal directly to an ESP32 GPIO.
 * Serial Monitor: 115200 baud
 */

#include <Arduino.h>

const uint8_t sensorPins[7] = {13, 14, 25, 26, 27, 33, 34};
const int weights[7] = {-3000, -2000, -1000, 0, 1000, 2000, 3000};
const bool invertSensorLogic = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  for (uint8_t i = 0; i < 7; i++) {
    pinMode(sensorPins[i], INPUT);
  }

  Serial.println("\nDIRECT 7-CHANNEL DIGITAL IR TEST");
  Serial.println("Move black tape under each sensor at 2-5 mm height.");
  Serial.println("Expected after inversion: black=1, white=0.");
  Serial.println("S1 S2 S3 | S4 S5 S6 S7 | POSITION");
}

void loop() {
  long weightedSum = 0;
  int activeSensors = 0;

  Serial.print("    ");
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t value = digitalRead(sensorPins[i]);
    if (invertSensorLogic) {
      value = !value;
    }

    if (i == 3) {
      Serial.print("| ");
    }
    Serial.printf("%u ", value);

    if (value == 1) {
      weightedSum += weights[i];
      activeSensors++;
    }
  }

  if (activeSensors == 0) {
    Serial.println("| LINE LOST");
  } else {
    Serial.printf("| %d (active=%d)\n",
                  (int)(weightedSum / activeSensors), activeSensors);
  }

  delay(200);
}
