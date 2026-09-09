/*
 * PROJECT VARUNA 1.0 — PCF8574 Dual Expander QTR-8 Line Sensor Module
 * Header File — With Calibration Support
 */

#ifndef PCF8574_SENSORS_H
#define PCF8574_SENSORS_H

#include <Arduino.h>
#include "Config.h"

enum LineSide {
  SIDE_LEFT,
  SIDE_RIGHT
};

class PCF8574_Sensors {
public:
  PCF8574_Sensors();

  void begin();
  bool update();

  // ---- Calibration API ----
  // Phase 1: Call with sensors over WHITE surface (press button to trigger)
  void calibrateWhite();
  // Phase 2: Call with sensors over BLACK line (press button to trigger)
  void calibrateBlack();
  // Returns true if both white and black phases are complete
  bool isCalibrated() const { return _whiteCalibrated && _blackCalibrated; }
  // Resets calibration so it can be redone
  void resetCalibration();

  int getPosition() const { return _positionError; }
  int getActiveCount() const { return _activeSensors; }
  bool isLineLost() const { return _lineLost; }
  LineSide getLastLineSide() const { return _lastLineSide; }
  uint8_t getRaw(uint8_t index) const { return (index < SENSOR_COUNT) ? _rawSensors[index] : 0; }

private:
  uint8_t _addr1;
  uint8_t _addr2;
  uint8_t _rawSensors[SENSOR_COUNT];       // Final calibrated binary sensor values
  uint8_t _rawAnalog[SENSOR_COUNT];        // Raw digital value before calibration
  int _whiteReading[SENSOR_COUNT];         // Average raw count when over white background
  int _blackReading[SENSOR_COUNT];         // Average raw count when over black line
  int _threshold[SENSOR_COUNT];            // Mid-point threshold per channel
  bool _whiteCalibrated;
  bool _blackCalibrated;
  bool _invertChannel[SENSOR_COUNT];       // Per-channel polarity

  int _positionError;
  int _lastPositionError;
  int _activeSensors;
  bool _lineLost;
  LineSide _lastLineSide;

  void readRawAnalog(int readings[SENSOR_COUNT]);
};

#endif // PCF8574_SENSORS_H
