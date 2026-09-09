/*
 * PROJECT VARUNA 1.0 — AUTONOMOUS LINE FOLLOWER COMPETITION FIRMWARE
 * Event: Mini Electric Vehicle Competition 3.0 (Main Event: Line Follower Race)
 * Target MCU: ESP32 38-Pin Classic WROOM Board
 *
 * Peripherals:
 * - 7-channel digital IR array connected directly to ESP32 GPIOs
 * - 2 x BTS7960 Motor Drivers (Left: GPIO18/19, Right: GPIO16/17)
 * - 4 x 400 RPM DC Motors (100mm Wheels, 12V Battery)
 * - 1 x Calibration Button on GPIO32 (wire between GPIO32 and GND)
 *
 * Hardware Mapping:
 * - PCF #1 (0x20): Module 1 (LEFT)  — M1-D8 to M1-D1
 * - PCF #2 (0x24): Module 2 (RIGHT) — M2-D8 to M2-D1
 * - Sensor Weights: -7500 (M1-D8) to +7500 (M2-D1), Center = 0
 *
 * CALIBRATION:
 * On power-up, robot enters calibration mode:
 *   STEP 1: Hold sensors over WHITE surface → Press CALIBRATION BUTTON
 *   STEP 2: Hold sensors over BLACK line   → Press CALIBRATION BUTTON
 *   Robot then starts autonomous line following automatically!
 *
 * Loop Execution Rate: 400 Hz (2500 microseconds loop interval)
 * Arduino-ESP32 Core 3.x Compliant
 */

#include <Arduino.h>
#include "Config.h"
#include "PCF8574_Sensors.h"
#include "MotorDriver.h"
#include "PIDController.h"
#include "Buzzer.h"

// Objects
PCF8574_Sensors sensors;
MotorDriver motors;
PIDController pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
Buzzer buzzer;

// Execution Timing
unsigned long lastLoopTimeMicros = 0;
const float dtSeconds = (float)CONTROL_LOOP_MICROS / 1000000.0f; // 0.0025s

// Diagnostic Telemetry Flag
bool enableTelemetry = false;

// Line-lost buzzer rate limiter (avoid blocking loop with repeated beeps)
bool wasLineLost = false;

// -----------------------------------------------------------------------
// waitForButtonPress: Blocking wait until the calibration button is pressed
// and released, with 50 ms debounce.
// GPIO32 has internal pull-up support; button connects GPIO32 to GND.
// -----------------------------------------------------------------------
void waitForButtonPress() {
  Serial.printf("[BUTTON] GPIO%d is %s. Press button (connect GPIO%d to GND).\n",
                CALIBRATION_BUTTON_PIN,
                digitalRead(CALIBRATION_BUTTON_PIN) == HIGH ? "HIGH/RELEASED" : "LOW/PRESSED",
                CALIBRATION_BUTTON_PIN);

  // Wait for button to be released first (in case already held)
  while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(10);
  }

  Serial.println("[BUTTON] Waiting for press...");

  // Wait for press (active LOW)
  while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
    delay(10);
  }

  // Confirm that the signal remains LOW after debounce.
  delay(50);
  if (digitalRead(CALIBRATION_BUTTON_PIN) != LOW) {
    Serial.println("[BUTTON] Electrical bounce detected; waiting again.");
    return waitForButtonPress();
  }

  Serial.println("[BUTTON] Press detected. Release button...");

  // Wait for release
  while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(10);
  }
  delay(50); // Post-release debounce
  Serial.println("[BUTTON] Released.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // GPIO32 supports INPUT_PULLUP — NO external resistor needed.
  // Button connects GPIO32 to GND (active LOW when pressed).
  pinMode(CALIBRATION_BUTTON_PIN, INPUT_PULLUP);

  // 0. Initialize Buzzer (GPIO25)
  buzzer.begin();

  Serial.println("\n========================================================");
  Serial.println("  PROJECT VARUNA 1.0 — LINE FOLLOWER COMPETITION CORE  ");
  Serial.println("========================================================");

  // Boot chime — rising 4-step tone sequence
  buzzer.bootChime();
  Serial.println("[OK] Buzzer Online — Boot Chime Played.");

  // 1. Initialize Motors
  motors.begin();
  motors.stop(); // Safety: ensure motors are stopped during calibration
  Serial.println("[OK] BTS7960 Motor Drivers Initialized.");

  // 2. Initialize direct digital IR sensors
  sensors.begin();

  // 3. Reset PID Controller
  pid.reset();
  Serial.println("[OK] PID Controller Loaded.");

  // -----------------------------------------------------------------------
  // CALIBRATION SEQUENCE
  // -----------------------------------------------------------------------
  Serial.println("\n========================================================");
  Serial.println("           CALIBRATION REQUIRED BEFORE RACING           ");
  Serial.println("========================================================");
  Serial.println("STEP 1: Place the robot on WHITE background.");
  Serial.println("        Press the CALIBRATION BUTTON (GPIO32) to sample WHITE.");

  waitForButtonPress();
  sensors.calibrateWhite();
  buzzer.beepWhiteDone(); // 1 beep — white done!

  Serial.println("\nSTEP 2: Place the robot over the BLACK line.");
  Serial.println("        Press the CALIBRATION BUTTON (GPIO32) to sample BLACK.");

  waitForButtonPress();
  sensors.calibrateBlack();
  buzzer.beepBlackDone(); // 2 beeps — black done!

  Serial.println("\n========================================================");
  Serial.println("  CALIBRATION COMPLETE! Starting race in 3 seconds...  ");
  Serial.println("========================================================\n");

  delay(500);
  buzzer.beepCalibrationReady(); // Victory triple rising beep — RACE START!
  delay(2500); // Total ~3s before race starts

  lastLoopTimeMicros = micros();
}


void loop() {
  // Maintain precise 2500 microsecond (400 Hz) control loop timing
  unsigned long nowMicros = micros();
  if (nowMicros - lastLoopTimeMicros < CONTROL_LOOP_MICROS) {
    return;
  }
  lastLoopTimeMicros = nowMicros;

  // 1. Update Sensor Readings
  sensors.update();

  int leftMotorSpeed = 0;
  int rightMotorSpeed = 0;

  // 2. Check Line Status & Execute Steering Strategy
  if (sensors.isLineLost()) {
    // -------------------------------------------------------------------------
    // ALGORITHM STATE 1: LINE LOST RECOVERY
    // -------------------------------------------------------------------------
    // All 10 sensors read white (0). Execute spin recovery based on last known side.
    if (!wasLineLost) {
      // Only beep ONCE when line is first lost, not every loop tick
      buzzer.beepLineLost();
      wasLineLost = true;
    }

    if (sensors.getLastLineSide() == SIDE_LEFT) {
      // Line was lost off the left edge -> Spin Left to re-acquire line
      leftMotorSpeed  = LINE_LOSS_INNER_PWM; // -55
      rightMotorSpeed = LINE_LOSS_OUTER_PWM; // +80
    } else {
      // Line was lost off the right edge -> Spin Right to re-acquire line
      leftMotorSpeed  = LINE_LOSS_OUTER_PWM; // +80
      rightMotorSpeed = LINE_LOSS_INNER_PWM; // -55
    }
    pid.reset(); // Reset PID integral and derivative tracking while searching

  } else {
    wasLineLost = false; // Line re-acquired — reset line-lost flag
    // -------------------------------------------------------------------------
    // ALGORITHM STATE 2: ACTIVE LINE TRACKING
    // -------------------------------------------------------------------------
    int error = sensors.getPosition();
    int absError = abs(error);

    if (absError > ERROR_SHARP_THRESH) {
      // -----------------------------------------------------------------------
      // HARD/SHARP TURN OVERRIDE (|Error| > 6000)
      // -----------------------------------------------------------------------
      if (error < 0) {
        // Sharp turn LEFT required (Error < -6000)
        leftMotorSpeed  = SHARP_TURN_INNER_PWM; // -45
        rightMotorSpeed = SHARP_TURN_OUTER_PWM; // +125
      } else {
        // Sharp turn RIGHT required (Error > +6000)
        leftMotorSpeed  = SHARP_TURN_OUTER_PWM; // +125
        rightMotorSpeed = SHARP_TURN_INNER_PWM; // -45
      }

    } else {
      // -----------------------------------------------------------------------
      // HIGH-SPEED PID LINE FOLLOWING (Adaptive Base Speed Curve)
      // -----------------------------------------------------------------------
      // Dynamic base speed scaling based on track curvature:
      // - Straight line (Error < 800)  -> 255 PWM (Full Maximum Speed)
      // - Sweeping curve (Error < 2000) -> 210 PWM
      // - Mild turn (Error < 4000)     -> 165 PWM
      // - Medium turn (Error < 6000)   -> 125 PWM
      int currentBaseSpeed = SPEED_SHARP_TURN;

      if (absError < ERROR_STRAIGHT_THRESH) {
        currentBaseSpeed = SPEED_STRAIGHT;   // 255 PWM (MAX SPEED BURST)
      } else if (absError < ERROR_SWEEP_THRESH) {
        currentBaseSpeed = SPEED_SWEEP_TURN;  // 210 PWM (Fast Sweeper)
      } else if (absError < ERROR_SMALL_THRESH) {
        currentBaseSpeed = SPEED_SMALL_TURN;  // 165 PWM
      } else if (absError < ERROR_MED_THRESH) {
        currentBaseSpeed = SPEED_MED_TURN;    // 125 PWM
      }

      // Compute PID Correction
      int correction = pid.compute(error, dtSeconds);

      // Differential Motor Speed Calculation
      leftMotorSpeed  = currentBaseSpeed + correction;
      rightMotorSpeed = currentBaseSpeed - correction;
    }
  }

  // 3. Command Motors
  motors.drive(leftMotorSpeed, rightMotorSpeed);

  // 4. Optional Serial Telemetry (Enable via Serial commands if needed)
  if (enableTelemetry) {
    Serial.printf("Err: %5d | Active: %2d | Lost: %d | L_PWM: %4d | R_PWM: %4d\n",
                  sensors.getPosition(), sensors.getActiveCount(), 
                  sensors.isLineLost(), leftMotorSpeed, rightMotorSpeed);
  }
}
