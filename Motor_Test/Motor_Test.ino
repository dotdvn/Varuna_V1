/*
 * PROJECT VARUNA 1.0 — BTS7960 Motor Driver Test Utility
 * Competition: Mini Electric Vehicle Competition 3.0 (Line Follower Race)
 * Hardware: ESP32 38-Pin Classic WROOM Board + 2 x BTS7960 Motor Drivers
 * 
 * Pinout:
 * - LEFT BTS7960 (#1):  RPWM = GPIO18, LPWM = GPIO19
 * - RIGHT BTS7960 (#2): RPWM = GPIO16, LPWM = GPIO17
 * 
 * PWM Specs:
 * - Frequency:  20,000 Hz (20 kHz)
 * - Resolution: 8-bit (0 - 255)
 * - ESP32 Core 3.x API: ledcAttach(pin, freq, resolution),
 *   ledcWrite(pin, duty)
 * 
 * Serial Monitor Baud Rate: 115200
 */

#include <Arduino.h>

// Pin Definitions
#define LEFT_RPWM  18
#define LEFT_LPWM  19
#define RIGHT_RPWM 16
#define RIGHT_LPWM 17

// PWM Setup
#define PWM_FREQ 20000
#define PWM_RES  8
#define TEST_PWM 255 // Full speed for bench testing

void setLeftMotor(int speed) {
  // speed range: -255 to +255
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    ledcWrite(LEFT_RPWM, 0);
    ledcWrite(LEFT_LPWM, speed);
  } else if (speed < 0) {
    ledcWrite(LEFT_RPWM, -speed);
    ledcWrite(LEFT_LPWM, 0);
  } else {
    ledcWrite(LEFT_RPWM, 0);
    ledcWrite(LEFT_LPWM, 0);
  }
}

void setRightMotor(int speed) {
  // speed range: -255 to +255
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    ledcWrite(RIGHT_RPWM, 0);
    ledcWrite(RIGHT_LPWM, speed);
  } else if (speed < 0) {
    ledcWrite(RIGHT_RPWM, -speed);
    ledcWrite(RIGHT_LPWM, 0);
  } else {
    ledcWrite(RIGHT_RPWM, 0);
    ledcWrite(RIGHT_LPWM, 0);
  }
}

void stopMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n========================================================");
  Serial.println("   PROJECT VARUNA 1.0 — BTS7960 MOTOR DRIVER TEST      ");
  Serial.println("========================================================");

  ledcAttach(LEFT_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(LEFT_LPWM, PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_LPWM, PWM_FREQ, PWM_RES);

  stopMotors();
  Serial.println("Motor channels attached at 20 kHz, 8-bit PWM.");
  Serial.println("Starting test sequence in 2 seconds...");
  delay(2000);
}

void loop() {
  // Test 1: Both Motors Forward
  Serial.println("---> 1. TESTING FORWARD (PWM: 80)");
  setLeftMotor(TEST_PWM);
  setRightMotor(TEST_PWM);
  delay(2000);

  stopMotors();
  delay(1000);

  // Test 2: Both Motors Reverse
  Serial.println("---> 2. TESTING REVERSE (PWM: -80)");
  setLeftMotor(-TEST_PWM);
  setRightMotor(-TEST_PWM);
  delay(2000);

  stopMotors();
  delay(1000);

  // Test 3: Spin Turn Left (Left Reverse, Right Forward)
  Serial.println("---> 3. TESTING SPIN TURN LEFT (Left -80, Right +80)");
  setLeftMotor(-TEST_PWM);
  setRightMotor(TEST_PWM);
  delay(1500);

  stopMotors();
  delay(1000);

  // Test 4: Spin Turn Right (Left Forward, Right Reverse)
  Serial.println("---> 4. TESTING SPIN TURN RIGHT (Left +80, Right -80)");
  setLeftMotor(TEST_PWM);
  setRightMotor(-TEST_PWM);
  delay(1500);

  stopMotors();
  delay(1000);

  Serial.println("Test loop completed. Pausing for 5 seconds...\n");
  delay(5000);
}
