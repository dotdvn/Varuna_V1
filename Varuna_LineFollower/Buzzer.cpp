/*
 * PROJECT VARUNA 1.0 — Buzzer Module
 * Implementation File
 *
 * ESP32 Arduino Core 3.x LEDC implementation.
 */

#include "Buzzer.h"

Buzzer::Buzzer() : _toneOffAt(0), _lastSensorToneAt(0) {}

void Buzzer::begin() {
  ledcAttach(BUZZER_PIN, BUZZER_PWM_FREQ, BUZZER_PWM_RES);
  silence();
}

void Buzzer::tone(uint32_t freqHz, uint32_t durationMs) {
  ledcWriteTone(BUZZER_PIN, freqHz);
  delay(durationMs);
  silence();
}

void Buzzer::silence() {
  ledcWrite(BUZZER_PIN, 0);
}

void Buzzer::update() {
  if (_toneOffAt != 0 && (int32_t)(millis() - _toneOffAt) >= 0) {
    silence();
    _toneOffAt = 0;
  }
}

void Buzzer::sensorTriggered(uint8_t sensorIndex) {
  if (sensorIndex >= SENSOR_COUNT) {
    return;
  }

  uint32_t now = millis();
  if (now - _lastSensorToneAt < 35) {
    return;
  }

  // Left sensors are lower, right sensors higher, making direction audible.
  const uint16_t sensorTones[SENSOR_COUNT] = {
    330, 392, 440, 494, 554, 659, 740, 880
  };
  ledcWriteTone(BUZZER_PIN, sensorTones[sensorIndex]);
  _toneOffAt = now + 18;
  _lastSensorToneAt = now;
}

// =============================================================================
// BOOT TUNE - short rising arpeggio played on power-on.
// =============================================================================
void Buzzer::bootChime() {
  tone(262, 80); delay(25);   // C4
  tone(330, 80); delay(25);   // E4
  tone(392, 80); delay(25);   // G4
  tone(523, 140); delay(35);  // C5
  tone(659, 100); delay(25);  // E5
  tone(784, 220);             // G5
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
// CALIBRATION READY - race-start fanfare.
// =============================================================================
void Buzzer::beepCalibrationReady() {
  tone(392, 100); delay(25);   // G4
  tone(440, 100); delay(25);   // A4
  tone(494, 100); delay(25);   // B4
  tone(523, 180); delay(45);   // C5
  tone(659, 110); delay(25);   // E5
  tone(784, 110); delay(25);   // G5
  tone(1047, 360);             // C6
}

void Buzzer::beepIRMode() {
  tone(523, 120); delay(35);
  tone(659, 120); delay(35);
  tone(784, 240);
}

// =============================================================================
// LINE LOST WARNING — 1 short low-tone beep
// =============================================================================
void Buzzer::beepLineLost() {
  tone(BUZZER_TONE_LINELOST, 80);
}

void Buzzer::beepRadioMissing() {
  // Descending minor-style warning, approximately two seconds total.
  tone(440, 400); delay(50);
  tone(370, 400); delay(50);
  tone(311, 400); delay(50);
  tone(262, 600);
}
