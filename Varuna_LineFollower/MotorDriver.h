/*
 * PROJECT VARUNA 1.0 — BTS7960 Dual Motor Driver Module
 * Header File (ESP32 Core 3.x ledc API Compatible)
 */

#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "Config.h"

class MotorDriver {
public:
  MotorDriver();

  void begin();
  void drive(int leftSpeed, int rightSpeed);
  void stop();

private:
  void setLeftMotor(int speed);
  void setRightMotor(int speed);
};

#endif // MOTOR_DRIVER_H
