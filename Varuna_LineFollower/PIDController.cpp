/*
 * PROJECT VARUNA 1.0 — PID Steering Controller Module
 * Implementation File
 */

#include "PIDController.h"

PIDController::PIDController(float kp, float ki, float kd) 
  : _kp(kp), _ki(ki), _kd(kd), _integral(0.0f), _lastPosition(0), _firstRun(true) {}

void PIDController::setGains(float kp, float ki, float kd) {
  _kp = kp;
  _ki = ki;
  _kd = kd;
}

void PIDController::reset() {
  _integral = 0.0f;
  _lastPosition = 0;
  _firstRun = true;
}

int PIDController::compute(int currentPosition, float dtSeconds) {
  // Target Setpoint = 0 (Center of line)
  int error = currentPosition; // Error = Position - 0

  if (_firstRun) {
    _lastPosition = currentPosition;
    _firstRun = false;
  }

  // 1. Proportional Term
  float pTerm = _kp * (float)error;

  // 2. Integral Term with Anti-Windup Clamping
  _integral += (float)error * dtSeconds;
  // Limit integral windup to prevent runaway motor drift
  float maxIntegral = (float)MAX_PID_CORRECTION / (_ki > 0.000001f ? _ki : 1.0f);
  _integral = constrain(_integral, -maxIntegral, maxIntegral);
  float iTerm = _ki * _integral;

  // 3. Derivative Term (Derivative on Measurement to prevent setpoint kick)
  float derivative = (float)(currentPosition - _lastPosition) / (dtSeconds > 0.0001f ? dtSeconds : 0.0025f);
  float dTerm = _kd * derivative;

  _lastPosition = currentPosition;

  // Total Correction Calculation
  float totalOutput = pTerm + iTerm + dTerm;

  // Constrain total PID correction within bounds
  int correction = (int)constrain(totalOutput, -MAX_PID_CORRECTION, MAX_PID_CORRECTION);

  return correction;
}
