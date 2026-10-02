/*
 * PROJECT VARUNA 1.0 — BTS7960 Dual Motor Driver Module
 * Implementation File (ESP32 Core 3.x LEDC API)
 */

#include "MotorDriver.h"

MotorDriver::MotorDriver() {}

void MotorDriver::begin() {
  // Arduino-ESP32 Core 3.x attaches LEDC directly to each GPIO.
  ledcAttach(LEFT_MOTOR_RPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttach(LEFT_MOTOR_LPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttach(RIGHT_MOTOR_RPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttach(RIGHT_MOTOR_LPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);

  stop();
}

void MotorDriver::setLeftMotor(int speed) {
  speed = constrain(speed, MIN_PWM_LIMIT, MAX_PWM_LIMIT);
  if (speed > 0 && speed < MOTOR_START_PWM) speed = MOTOR_START_PWM;
  if (speed < 0 && speed > -MOTOR_START_PWM) speed = -MOTOR_START_PWM;
  if (speed > 0) {
    ledcWrite(LEFT_MOTOR_RPWM_PIN, 0);
    ledcWrite(LEFT_MOTOR_LPWM_PIN, speed);
  } else if (speed < 0) {
    ledcWrite(LEFT_MOTOR_RPWM_PIN, -speed);
    ledcWrite(LEFT_MOTOR_LPWM_PIN, 0);
  } else {
    ledcWrite(LEFT_MOTOR_RPWM_PIN, 0);
    ledcWrite(LEFT_MOTOR_LPWM_PIN, 0);
  }
}

void MotorDriver::setRightMotor(int speed) {
  speed = constrain(speed, MIN_PWM_LIMIT, MAX_PWM_LIMIT);
  if (speed > 0 && speed < MOTOR_START_PWM) speed = MOTOR_START_PWM;
  if (speed < 0 && speed > -MOTOR_START_PWM) speed = -MOTOR_START_PWM;
  if (speed > 0) {
    ledcWrite(RIGHT_MOTOR_RPWM_PIN, 0);
    ledcWrite(RIGHT_MOTOR_LPWM_PIN, speed);
  } else if (speed < 0) {
    ledcWrite(RIGHT_MOTOR_RPWM_PIN, -speed);
    ledcWrite(RIGHT_MOTOR_LPWM_PIN, 0);
  } else {
    ledcWrite(RIGHT_MOTOR_RPWM_PIN, 0);
    ledcWrite(RIGHT_MOTOR_LPWM_PIN, 0);
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
