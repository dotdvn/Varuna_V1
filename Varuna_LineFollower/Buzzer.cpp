/*
 * PROJECT VARUNA 1.0 — Buzzer Module
 * Implementation File
 *
 * ESP32 Arduino Core compatible LEDC implementation.
 * Uses ledcSetup/ledcAttachPin for the installed core, since ledcAttach is not available.
 */

#include "Buzzer.h"

Buzzer::Buzzer() {}

void Buzzer::begin() {
  ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQ, BUZZER_PWM_RES);
  ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
  silence();
}

void Buzzer::tone(uint32_t freqHz, uint32_t durationMs) {
  // ledcWriteTone must target the LEDC channel, not the GPIO pin.
  ledcWriteTone(BUZZER_PWM_CHANNEL, freqHz);
  delay(durationMs);
  silence();
}

void Buzzer::silence() {
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
}

// =============================================================================
// BOOT CHIME — Rising 3-step tone played on power-on
// Sounds like a "ready" startup sequence
// =============================================================================
void Buzzer::bootChime() {
  tone(800,  100); delay(40);
  tone(1200, 100); delay(40);
  tone(1600, 100); delay(40);
  tone(2000, 180);
}

// =============================================================================
// WHITE CALIBRATION DONE — 1 short mid-tone beep
// =============================================================================
void Buzzer::beepWhiteDone() {
  tone(BUZZER_TONE_WHITE_CAL, 150);
}

// =============================================================================
// BLACK CALIBRATION DONE — 2 quick beeps
// =============================================================================
void Buzzer::beepBlackDone() {
  tone(BUZZER_TONE_BLACK_CAL, 120); delay(80);
  tone(BUZZER_TONE_BLACK_CAL, 120);
}

// =============================================================================
// CALIBRATION READY (RACE START) — Victory triple rising beep
// Plays just before autonomous driving begins!
// =============================================================================
void Buzzer::beepCalibrationReady() {
  tone(1000, 100); delay(60);
  tone(1400, 100); delay(60);
  tone(2000, 300);
}

// =============================================================================
// LINE LOST WARNING — 1 short low-tone beep
// =============================================================================
void Buzzer::beepLineLost() {
  tone(BUZZER_TONE_LINELOST, 80);
}
