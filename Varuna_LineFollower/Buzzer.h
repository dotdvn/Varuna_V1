/*
 * PROJECT VARUNA 1.0 — Buzzer Module
 * Header File
 *
 * GPIO4: PWM-capable, full GPIO, no boot issues, no resistor needed.
 * Wire: GPIO4 → Buzzer (+) | GND → Buzzer (-)
 *
 * Uses ESP32 Arduino Core 3.x ledcAttach / ledcWrite API.
 * Supports tones, beep patterns, and the VARUNA boot chime.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "Config.h"

class Buzzer {
public:
  Buzzer();
  void begin();

  // Single tone: plays for durationMs milliseconds then stops
  void tone(uint32_t freqHz, uint32_t durationMs);

  // Silence the buzzer immediately
  void silence();

  // Pre-defined beep events
  void bootChime();          // 3-step rising chime on power-on
  void beepWhiteDone();      // 1 short beep — white calibration done
  void beepBlackDone();      // 2 short beeps — black calibration done
  void beepCalibrationReady(); // Victory triple-beep — fully calibrated & ready to race!
  void beepLineLost();       // 1 short low-tone warning — line lost
};

#endif // BUZZER_H
