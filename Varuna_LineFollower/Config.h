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

// Sensor output: White = HIGH/LED ON, Black = LOW/LED OFF.
// After calibration, polarity is verified per sensor automatically.
#define INVERT_SENSORS true

// =============================================================================
// CALIBRATION BUTTON
// =============================================================================
// Vehicle PCF8574: wire the calibration button between P0 and GND.
#define PCF8574_ADDRESS       0x27
#define PCF_CAL_BUTTON_PIN    0
#define CALIBRATION_SAMPLES    80  // Number of readings taken per phase
#define CALIBRATION_DELAY_MS   25  // Delay between each calibration sample (ms)
#define CALIBRATION_MIN_DELTA  20  // Minimum white/black sample-count difference

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
#define ERROR_STRAIGHT_THRESH  500 // Error < 500 = straight
#define ERROR_SWEEP_THRESH    1200 // Error < 1200 = sweeping turn
#define ERROR_SMALL_THRESH    2000 // Error < 2000 = mild turn
#define ERROR_MED_THRESH      2800 // Error < 2800 = medium turn
#define ERROR_SHARP_THRESH    2800 // Error >= 2800 = sharp turn

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

// =============================================================================
// REMOTE RADIO — native control pins. GPIO32 is reserved for radio CSN.
// =============================================================================
#define NRF_CSN_PIN  32
#define NRF_CE_PIN   23
#define NRF_SCK_PIN   5
#define NRF_MISO_PIN 12
#define NRF_MOSI_PIN 15
#define RADIO_TIMEOUT_MS 350
#define RADIO_CONNECT_GRACE_MS 5000

// =============================================================================
// TELEMETRY
// =============================================================================
#define MPU_SDA_PIN 21
#define MPU_SCL_PIN 22
#define GPS_RX_PIN  39
#define BATTERY_ADC_PIN 36

// Common 0-25 V sensor boards divide input voltage by 5. Adjust after checking
// against a multimeter. The battery ADC input must never exceed 3.3 V.
#define BATTERY_DIVIDER_RATIO 5.0f
#define BATTERY_CALIBRATION    1.0f
#define BATTERY_LOW_MV        10500
#define TELEMETRY_PERIOD_MS   100

#endif // CONFIG_H
