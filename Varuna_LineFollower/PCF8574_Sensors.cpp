/*
 * PROJECT VARUNA 1.0 — Direct Digital IR Line Sensor Module
 * Implementation File — With Calibration Support
 *
 * CALIBRATION FLOW:
 * 1. Power ON robot.
 * 2. Hold sensors over WHITE background. Press calibration button.
 *    -> Serial prints "Calibrating WHITE... Done!"
 * 3. Place sensors over BLACK line. Press calibration button again.
 *    -> Serial prints "Calibrating BLACK... Done! Calibration COMPLETE!"
 * 4. Robot is now ready for autonomous line following!
 *
 * Per-channel auto-polarity detection:
 * If a sensor reads higher on BLACK than WHITE -> it is normal polarity (black=1).
 * If a sensor reads higher on WHITE than BLACK -> it is inverted (auto-corrected).
 * This makes the system work with ANY QTR-8 variant or clone automatically.
 */

#include "PCF8574_Sensors.h"

PCF8574_Sensors::PCF8574_Sensors()
  : _positionError(0), _lastPositionError(0),
    _activeSensors(0), _lineLost(false), _lastLineSide(SIDE_LEFT),
    _whiteCalibrated(false), _blackCalibrated(false) {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    _rawSensors[i]    = 0;
    _rawAnalog[i]     = 0;
    _whiteReading[i]  = 0;
    _blackReading[i]  = 0;
    _threshold[i]     = 1; // Default threshold midpoint
    _invertChannel[i] = false;
  }
}

void PCF8574_Sensors::begin() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  Serial.println("[OK] 7 digital IR sensors initialized (no PCF8574/I2C).");
}

// Internal helper: reads the seven connected sensors into readings[7].
void PCF8574_Sensors::readRawAnalog(int readings[SENSOR_COUNT]) {
  // Accumulate multiple samples for stable readings during calibration.
  long accum[SENSOR_COUNT] = {0};
  for (int s = 0; s < CALIBRATION_SAMPLES; s++) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
      accum[i] += digitalRead(SENSOR_PINS[i]);
    }
    delay(CALIBRATION_DELAY_MS);
  }
  // Average
  for (int i = 0; i < SENSOR_COUNT; i++) {
    readings[i] = (int)(accum[i]);
  }
}

void PCF8574_Sensors::calibrateWhite() {
  Serial.println("[CAL] Place sensors over WHITE background...");
  Serial.println("[CAL] Sampling WHITE surface...");
  readRawAnalog(_whiteReading);
  _whiteCalibrated = true;
  Serial.println("[CAL] WHITE calibration done!");
  Serial.println("[CAL] Now place sensors over BLACK line and press button again.");
}

void PCF8574_Sensors::calibrateBlack() {
  Serial.println("[CAL] Sampling BLACK line...");
  readRawAnalog(_blackReading);
  _blackCalibrated = true;

  // Compute per-channel threshold and auto-detect polarity
  for (int i = 0; i < SENSOR_COUNT; i++) {
    _threshold[i]     = (_whiteReading[i] + _blackReading[i]) / 2;
    // If white reading count is HIGHER than black reading count:
    // the sensor outputs HIGH on white (inverted behavior) -> auto-invert this channel
    _invertChannel[i] = (_whiteReading[i] > _blackReading[i]);
  }

  Serial.println("[CAL] BLACK calibration done!");
  Serial.println("[CAL] ===== CALIBRATION COMPLETE! SENSOR POLARITY AUTO-DETECTED =====");

  // Print calibration summary
  Serial.println("[CAL] Per-channel summary (W=White count, B=Black count, Thr=Threshold, Inv=Inverted):");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.printf("  CH%02d: W=%3d B=%3d Thr=%2d Inv=%s\n",
                  i + 1, _whiteReading[i], _blackReading[i],
                  _threshold[i], _invertChannel[i] ? "YES" : "NO");
  }
}

void PCF8574_Sensors::resetCalibration() {
  _whiteCalibrated = false;
  _blackCalibrated = false;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    _whiteReading[i]  = 0;
    _blackReading[i]  = 0;
    _threshold[i]     = 1;
    _invertChannel[i] = false;
  }
  Serial.println("[CAL] Calibration has been reset.");
}

bool PCF8574_Sensors::update() {
  uint8_t rawBits[SENSOR_COUNT];
  for (int i = 0; i < SENSOR_COUNT; i++) {
    rawBits[i] = digitalRead(SENSOR_PINS[i]);
  }

  // Apply calibration or fallback to global INVERT_SENSORS
  for (int i = 0; i < SENSOR_COUNT; i++) {
    uint8_t val = rawBits[i];
    if (isCalibrated()) {
      // Use per-channel auto-detected polarity
      if (_invertChannel[i]) val = !val;
    } else {
      // Pre-calibration fallback
      if (INVERT_SENSORS) val = !val;
    }
    _rawSensors[i] = val;
  }

  // Calculate Weighted Center Position
  long weightedSum = 0;
  _activeSensors = 0;

  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (_rawSensors[i] == 1) { // Black line detected
      weightedSum += (long)SENSOR_WEIGHTS[i];
      _activeSensors++;
    }
  }

  if (_activeSensors > 0) {
    _positionError = (int)(weightedSum / _activeSensors);
    _lastPositionError = _positionError;
    _lineLost = false;

    if (_positionError < 0) {
      _lastLineSide = SIDE_LEFT;
    } else if (_positionError > 0) {
      _lastLineSide = SIDE_RIGHT;
    }
  } else {
    _lineLost = true;
  }

  return !_lineLost;
}
