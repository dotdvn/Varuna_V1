/*
 * PROJECT VARUNA 1.0 - Direct Digital IR Line Sensor Module
 * Header File - With Calibration Support
 */

#ifndef DIGITAL_IR_SENSORS_H
#define DIGITAL_IR_SENSORS_H

#include <Arduino.h>
#include "Config.h"

enum LineSide {
  SIDE_LEFT,
  SIDE_RIGHT
};

class DigitalIR_Sensors {
public:
  DigitalIR_Sensors();

  void begin();
  bool update();

  void calibrateWhite();
  void calibrateBlack();
  bool isCalibrated() const {
    return _whiteCalibrated && _blackCalibrated && _calibrationValid;
  }
  void resetCalibration();

  int getPosition() const { return _positionError; }
  int getActiveCount() const { return _activeSensors; }
  bool isLineLost() const { return _lineLost; }
  LineSide getLastLineSide() const { return _lastLineSide; }
  uint8_t getRaw(uint8_t index) const { return (index < SENSOR_COUNT) ? _rawSensors[index] : 0; }

private:
  uint8_t _rawSensors[SENSOR_COUNT];
  int _whiteReading[SENSOR_COUNT];
  int _blackReading[SENSOR_COUNT];
  int _threshold[SENSOR_COUNT];
  bool _whiteCalibrated;
  bool _blackCalibrated;
  bool _calibrationValid;
  bool _invertChannel[SENSOR_COUNT];

  int _positionError;
  int _lastPositionError;
  int _activeSensors;
  bool _lineLost;
  LineSide _lastLineSide;

  void readRawValues(int readings[SENSOR_COUNT]);
};

#endif // DIGITAL_IR_SENSORS_H
