/*
 * PROJECT VARUNA 1.0 — BTS7960 Dual Motor Driver Module
 * Implementation File (ESP32 Core 3.x ledc API Compatible)
 */

#include "MotorDriver.h"

MotorDriver::MotorDriver() {}

void MotorDriver::begin() {
  // Assign channels 0 to 3 for motors
  ledcSetup(0, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttachPin(LEFT_MOTOR_RPWM_PIN, 0);

  ledcSetup(1, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttachPin(LEFT_MOTOR_LPWM_PIN, 1);

  ledcSetup(2, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttachPin(RIGHT_MOTOR_RPWM_PIN, 2);

  ledcSetup(3, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttachPin(RIGHT_MOTOR_LPWM_PIN, 3);

  stop();
}

void MotorDriver::setLeftMotor(int speed) {
  speed = constrain(speed, MIN_PWM_LIMIT, MAX_PWM_LIMIT);
  if (speed > 0) {
    ledcWrite(0, speed);
    ledcWrite(1, 0);
  } else if (speed < 0) {
    ledcWrite(0, 0);
    ledcWrite(1, -speed);
  } else {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
  }
}

void MotorDriver::setRightMotor(int speed) {
  speed = constrain(speed, MIN_PWM_LIMIT, MAX_PWM_LIMIT);
  if (speed > 0) {
    ledcWrite(2, speed);
    ledcWrite(3, 0);
  } else if (speed < 0) {
    ledcWrite(2, 0);
    ledcWrite(3, -speed);
  } else {
    ledcWrite(2, 0);
    ledcWrite(3, 0);
  }
}

void MotorDriver::drive(int leftSpeed, int rightSpeed) {
  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);
}

void MotorDriver::stop() {
  setLeftMotor(0);
  setRightMotor(0);
}
