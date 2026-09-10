/*
 * PROJECT VARUNA 1.0 — System Configuration Header
 * Competition: Mini Electric Vehicle Competition 3.0 (Line Follower Race)
 * Target Hardware: ESP32 38-Pin + 8x digital IR sensors + 2x BTS7960 Drivers
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// DIGITAL IR SENSOR CONFIGURATION
// =============================================================================
#define SENSOR_COUNT 8
// Physical order LEFT -> RIGHT. These pins avoid ESP32 boot-strapping pins,
// flash pins, serial pins, motor pins, buzzer pin, and the calibration button.
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = {
  13, 14, 25, 26, 27, 33, 34, 35
};

// Logic Polarity (Default: White = 0, Black = 1)
// Note: After calibration, polarity is determined automatically.
#define INVERT_SENSORS false

// =============================================================================
// CALIBRATION BUTTON
// =============================================================================
// GPIO32: Full GPIO with internal INPUT_PULLUP support. NO external resistor needed!
// Wire button between GPIO32 and GND. Internal pullup keeps pin HIGH until pressed.
#define CALIBRATION_BUTTON_PIN 32
#define CALIBRATION_SAMPLES    80  // Number of readings taken per phase
#define CALIBRATION_DELAY_MS   25  // Delay between each calibration sample (ms)

// =============================================================================
// BUZZER CONFIGURATION
// =============================================================================
// GPIO4: Full GPIO, PWM-capable, no boot/strapping issues. No external resistor needed.
// Wire: GPIO4 -> Buzzer (+) / Buzzer (-) -> GND
// Use a 5V passive piezo buzzer for best volume. Active buzzers also work.
#define BUZZER_PIN            4

// Buzzer Tones (Hz)
#define BUZZER_TONE_BOOT     1200  // Boot chime frequency
#define BUZZER_TONE_WHITE_CAL 800  // White calibration done beep
#define BUZZER_TONE_BLACK_CAL 1000 // Black calibration done beep
#define BUZZER_TONE_READY    1800  // Full calibration complete (startup ready)
#define BUZZER_TONE_LINELOST  400  // Low warning tone for line lost

// Buzzer PWM channel (must not conflict with motor PWM channels)
// Motors use channels 0-3 on GPIOs 16,17,18,19. GPIO25 is separate.
#define BUZZER_PWM_CHANNEL   4     // LEDC channel reserved for the buzzer
#define BUZZER_PWM_FREQ      2000  // Default buzzer carrier frequency (Hz)
#define BUZZER_PWM_RES       8     // 8-bit resolution

// =============================================================================
// MOTOR DRIVER PINOUT (BTS7960 High-Power Drivers)
// =============================================================================
#define LEFT_MOTOR_RPWM_PIN  18
#define LEFT_MOTOR_LPWM_PIN  19
#define RIGHT_MOTOR_RPWM_PIN 16
#define RIGHT_MOTOR_LPWM_PIN 17

// PWM Settings (Arduino ESP32 Core 3.x API compatible)
#define MOTOR_PWM_FREQ 20000 // 20 kHz ultrasonic frequency (silent operation)
#define MOTOR_PWM_RES  8     // 8-bit resolution (0 to 255)

// =============================================================================
// SENSOR WEIGHTS & GEOMETRY
// =============================================================================
static const int SENSOR_WEIGHTS[SENSOR_COUNT] = {
  -3500, -2500, -1500, -500, +500, +1500, +2500, +3500
};

// =============================================================================
// PID GAINS & CONTROL TUNING (High-Speed Race Baseline)
// =============================================================================
#define DEFAULT_KP 0.050f
#define DEFAULT_KI 0.00002f
#define DEFAULT_KD 0.260f

#define MAX_PID_CORRECTION 220 // Maximum PWM deviation allowed from base speed

// =============================================================================
// HIGH-SPEED RACE DRIVE CONFIGURATION (PWM: 0 - 255)
// =============================================================================
#define SPEED_STRAIGHT    220 // Requested maximum straight-line speed
#define SPEED_SWEEP_TURN  180 // Sweeping turn
#define SPEED_SMALL_TURN  145 // Mild curve
#define SPEED_MED_TURN    110 // Medium curve
#define SPEED_SHARP_TURN   75 // Sharp curve base speed

#define MAX_PWM_LIMIT     255 // Maximum allowed PWM
#define MIN_PWM_LIMIT    -255 // Maximum reverse PWM limit for spin turns

// Thresholds for Error Classification
#define ERROR_STRAIGHT_THRESH  800 // Error < 800 = Full Straightaway 255 PWM
#define ERROR_SWEEP_THRESH    2000 // Error < 2000 = Sweeping turn (210 PWM)
#define ERROR_SMALL_THRESH    4000 // Error < 4000 = Mild turn (165 PWM)
#define ERROR_MED_THRESH      6000 // Error < 6000 = Medium turn (125 PWM)
#define ERROR_SHARP_THRESH    6000 // Error >= 6000 = Sharp turn condition

// =============================================================================
// LINE LOSS & RECOVERY SPEEDS
// =============================================================================
// If line lost to LEFT:  Left = -55, Right = 80
// If line lost to RIGHT: Left = 80,  Right = -55
#define LINE_LOSS_INNER_PWM -55
#define LINE_LOSS_OUTER_PWM  95

// Sharp turn override speeds (|Error| > 6000)
// Sharp Left  (Error < -6000): Left = -45, Right = 125
// Sharp Right (Error > +6000): Left = 125, Right = -45
#define SHARP_TURN_INNER_PWM -55
#define SHARP_TURN_OUTER_PWM 135

// =============================================================================
// SYSTEM LOOP TIMING
// =============================================================================
#define CONTROL_LOOP_MICROS 2500 // 2.5 milliseconds (400 Hz execution loop)

#endif // CONFIG_H
