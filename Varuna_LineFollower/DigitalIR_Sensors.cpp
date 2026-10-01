/*
 * PROJECT VARUNA 1.0 - Direct Digital IR Line Sensor Module
 * Implementation File - With Calibration Support
 */

#include "DigitalIR_Sensors.h"

DigitalIR_Sensors::DigitalIR_Sensors()
  : _positionError(0), _lastPositionError(0),
    _activeSensors(0), _lineLost(false), _lastLineSide(SIDE_LEFT),
    _whiteCalibrated(false), _blackCalibrated(false),
    _calibrationValid(false) {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    _rawSensors[i] = 0;
    _whiteReading[i] = 0;
    _blackReading[i] = 0;
    _threshold[i] = 1;
    _invertChannel[i] = false;
  }
}

void DigitalIR_Sensors::begin() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(SENSOR_PINS[i], INPUT);
  }
  Serial.println("[OK] 8 direct digital IR sensors initialized.");
}

void DigitalIR_Sensors::readRawValues(int readings[SENSOR_COUNT]) {
  long accum[SENSOR_COUNT] = {0};
  for (int sample = 0; sample < CALIBRATION_SAMPLES; sample++) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
      accum[i] += digitalRead(SENSOR_PINS[i]);
    }
    delay(CALIBRATION_DELAY_MS);
  }
  for (int i = 0; i < SENSOR_COUNT; i++) {
    readings[i] = (int)accum[i];
  }
}

void DigitalIR_Sensors::calibrateWhite() {
  Serial.println("[CAL] Place sensors over WHITE background...");
  Serial.println("[CAL] Sampling WHITE surface...");
  readRawValues(_whiteReading);
  _whiteCalibrated = true;
  Serial.println("[CAL] WHITE calibration done!");
  Serial.println("[CAL] Now place sensors over BLACK line and press button again.");
}

void DigitalIR_Sensors::calibrateBlack() {
  Serial.println("[CAL] Sampling BLACK line...");
  readRawValues(_blackReading);
  _blackCalibrated = true;
  _calibrationValid = true;
  bool hasContrast = false;

  for (int i = 0; i < SENSOR_COUNT; i++) {
    _threshold[i] = (_whiteReading[i] + _blackReading[i]) / 2;
    int delta = abs(_whiteReading[i] - _blackReading[i]);
    if (delta >= CALIBRATION_MIN_DELTA) {
      hasContrast = true;
      _invertChannel[i] = (_whiteReading[i] > _blackReading[i]);
    } else {
      // A narrow line may not reach every sensor during black sampling.
      _invertChannel[i] = INVERT_SENSORS;
    }
  }
  _calibrationValid = hasContrast;

  Serial.println("[CAL] BLACK calibration done!");
  if (_calibrationValid) {
    Serial.println("[CAL] ===== CALIBRATION COMPLETE! =====");
  } else {
    Serial.println("[CAL] FAILED: move every sensor over white and black surfaces.");
  }
  Serial.println("[CAL] Per-channel summary (W, B, threshold, inverted):");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.printf("  CH%02d: W=%3d B=%3d Thr=%2d Inv=%s\n",
                  i + 1, _whiteReading[i], _blackReading[i],
                  _threshold[i], _invertChannel[i] ? "YES" : "NO");
  }
}

void DigitalIR_Sensors::resetCalibration() {
  _whiteCalibrated = false;
  _blackCalibrated = false;
  _calibrationValid = false;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    _whiteReading[i] = 0;
    _blackReading[i] = 0;
    _threshold[i] = 1;
    _invertChannel[i] = false;
  }
  Serial.println("[CAL] Calibration has been reset.");
}

bool DigitalIR_Sensors::update() {
  uint8_t rawValues[SENSOR_COUNT];
  for (int i = 0; i < SENSOR_COUNT; i++) {
    rawValues[i] = digitalRead(SENSOR_PINS[i]);
  }

  for (int i = 0; i < SENSOR_COUNT; i++) {
    uint8_t value = rawValues[i];
    if (isCalibrated()) {
      if (_invertChannel[i]) {
        value = !value;
      }
    } else if (INVERT_SENSORS) {
      value = !value;
    }
    _rawSensors[i] = value;
  }

  long weightedSum = 0;
  _activeSensors = 0;
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (_rawSensors[i] == 1) {
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
