/*
 * PROJECT VARUNA 1.0 — PID Steering Controller Module
 * Header File
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"

class PIDController {
public:
  PIDController(float kp = DEFAULT_KP, float ki = DEFAULT_KI, float kd = DEFAULT_KD);

  void setGains(float kp, float ki, float kd);
  int compute(int currentPosition, float dtSeconds);
  void reset();

  float getKp() const { return _kp; }
  float getKi() const { return _ki; }
  float getKd() const { return _kd; }

private:
  float _kp;
  float _ki;
  float _kd;

  float _integral;
  int _lastPosition;
  bool _firstRun;
};

#endif // PID_CONTROLLER_H
