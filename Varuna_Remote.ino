/*
 * PROJECT VARUNA — ESP8266 HANDHELD REMOTE
 *
 * Hardware:
 *   NodeMCU ESP8266
 *   ST7735 1.8-inch TFT (128x160)
 *   nRF24L01+
 *   ADS1115 ADC
 *   Two-axis joystick, speed slider, and mode button
 *
 * The vehicle must use RadioProtocol.h and return TelemetryPacket as an
 * nRF24 acknowledgement payload.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ADS1X15.h>
#include "RadioProtocol.h"
#include "DotFrameMode.h"

using namespace VarunaRadio;

// -----------------------------------------------------------------------------
// Pin allocation
// -----------------------------------------------------------------------------
// Use raw ESP8266 GPIO numbers instead of D0-D8 board aliases so this also
// compiles when the Arduino IDE uses the Generic ESP8266 board definition.
#define NRF_CSN_PIN 16 // NodeMCU D0
#define NRF_CE_PIN  15 // NodeMCU D8

#define TFT_CS_PIN  4  // NodeMCU D2
#define TFT_DC_PIN  5  // NodeMCU D1
#define TFT_RST_PIN -1 // TFT RST is wired to the NodeMCU RST pin

#define I2C_SDA_PIN 2  // NodeMCU D4
#define I2C_SCL_PIN 0  // NodeMCU D3
#define BUZZER_PIN  1  // NodeMCU TX; Serial is ended before this becomes output

constexpr uint8_t PCF_MODE_BUTTON_BIT = 0;
constexpr uint8_t PCF_SELECT_BUTTON_BIT = 1;
constexpr uint8_t PCF8574_ADDRESS = 0x27;

// ADS1115 inputs
constexpr uint8_t ADC_SLIDER = 0;
// The joystick is mounted 90 degrees relative to its VRX/VRY labels, so its
// physical steering axis is on A3 and its physical drive axis is on A2.
constexpr uint8_t ADC_JOYSTICK_X = 3;
constexpr uint8_t ADC_JOYSTICK_Y = 2;

// ADS1115 GAIN_ONE = +/-4.096 V, 0.125 mV per count. At 3.3 V the useful
// single-ended range is about 0..26400 counts.
constexpr int16_t ADC_MIN = 0;
constexpr int16_t ADC_MAX = 26400;
constexpr uint16_t BUTTON_DEBOUNCE_MS = 45;
constexpr int16_t JOYSTICK_DEAD_ZONE = 90; // normalized units out of 1000

// Change these if an axis moves in the wrong direction.
constexpr bool INVERT_JOYSTICK_X = false;
constexpr bool INVERT_JOYSTICK_Y = false;
constexpr bool INVERT_SLIDER = false;

constexpr uint16_t COMMAND_PERIOD_MS = 25;  // 40 command packets/second
constexpr uint16_t SCREEN_PERIOD_MS = 120;  // about 8 screen updates/second
constexpr uint16_t LINK_TIMEOUT_MS = 700;

// Dashboard palette (RGB565). The dark background and bright status colours
// remain readable outdoors without turning the whole display into a glare.
constexpr uint16_t UI_BG = 0x0841;
constexpr uint16_t UI_PANEL = 0x10C3;
constexpr uint16_t UI_PANEL_EDGE = 0x2986;
constexpr uint16_t UI_MUTED = 0x8410;
constexpr uint16_t UI_TEAL = 0x05D6;
constexpr uint16_t UI_SKY = 0x5DDF;
constexpr uint16_t UI_ORANGE = 0xFD20;
constexpr uint16_t UI_RED = 0xF9C7;
constexpr uint16_t HORIZON_BLUE = 0x249F;
constexpr uint16_t HORIZON_RED = 0xD9C7;

// Used only to draw the battery gauge. Adjust these two values if the vehicle
// battery is not a three-cell (3S) lithium battery.
constexpr uint16_t BATTERY_EMPTY_MV = 10500;
constexpr uint16_t BATTERY_FULL_MV = 12600;

// Keep the SPI clock conservative for breadboard wiring and nRF24L01 clone
// modules. The vehicle uses the same speed.
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN, 2000000);
Adafruit_ST7735 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Adafruit_ADS1115 ads;
// A tiny off-screen buffer lets the moving joystick target appear atomically.
// It consumes only 1,922 bytes, unlike a risky full-screen ESP8266 framebuffer.
GFXcanvas16 joystickCanvas(31, 31);
GFXcanvas16 horizonCanvas(27, 27);

// Both ends must use the same five-byte address.
const uint8_t RADIO_ADDRESS[6] = "VRN01";

CommandPacket command = {};
TelemetryPacket telemetry = {};

int16_t joystickCenterX = ADC_MAX / 2;
int16_t joystickCenterY = ADC_MAX / 2;
bool lineFollowerMode = false;
bool lastRawButtonPressed = false;
bool debouncedButtonPressed = false;
uint32_t buttonChangedAt = 0;
uint32_t lastCommandAt = 0;
uint32_t lastScreenAt = 0;
uint32_t lastTelemetryAt = 0;
uint16_t packetsSent = 0;
uint16_t packetsAcknowledged = 0;
bool dashboardFrameDrawn = false;
bool forceDashboardRefresh = true;

enum AppMode : uint8_t {
  APP_REMOTE,
  APP_EASTER_MENU,
  APP_STAR_CATCH,
  APP_REACTION,
  APP_NEON_SNAKE,
  APP_ORBIT_DODGE,
  APP_DOTFRAME_ARMING,
  APP_DOTFRAME
};

AppMode appMode = APP_REMOTE;
bool selectRawState = false;
bool selectStableState = false;
bool selectLongHandled = false;
bool selectClick = false;
bool easterRequested = false;
bool easterExitRequested = false;
uint8_t wirelessCalibrationStep = 0;
uint8_t pendingCalibrationFlag = 0;
uint32_t selectChangedAt = 0;
uint32_t selectPressedAt = 0;
uint32_t dotFrameArmStarted = 0;
uint8_t easterMenuIndex = 0;
int16_t uiDrive = 0;
int16_t uiSteering = 0;

static int16_t clampAdc(int16_t value) {
  return constrain(value, ADC_MIN, ADC_MAX);
}

static bool beginPcf8574() {
  // Writing HIGH releases all quasi-bidirectional pins so the buttons on P0
  // and P1 can pull their inputs LOW.
  Wire.beginTransmission(PCF8574_ADDRESS);
  Wire.write(0xFF);
  return Wire.endTransmission() == 0;
}

static uint8_t readPcf8574() {
  if (Wire.requestFrom(PCF8574_ADDRESS, (uint8_t)1) != 1) {
    return 0xFF; // A bus error must never look like a button press.
  }
  return Wire.read();
}

static uint8_t readSpeedPwm() {
  long raw = clampAdc(ads.readADC_SingleEnded(ADC_SLIDER));
  if (INVERT_SLIDER) raw = ADC_MAX - raw;
  return (uint8_t)constrain(map(raw, ADC_MIN, ADC_MAX, 60, 225), 60, 225);
}

static int16_t normalizeJoystick(int16_t raw, int16_t center, bool invert) {
  raw = clampAdc(raw);
  long value;

  if (raw >= center) {
    value = map(raw, center, ADC_MAX, 0, 1000);
  } else {
    value = map(raw, ADC_MIN, center, -1000, 0);
  }

  value = constrain(value, -1000, 1000);
  if (abs(value) < JOYSTICK_DEAD_ZONE) value = 0;
  if (invert) value = -value;
  return (int16_t)value;
}

static void drawBootProgress(uint8_t percent,
                             const __FlashStringHelper *status) {
  percent = constrain(percent, 0, 100);

  // The second boot screen is a compact diagnostic console. Redrawing only
  // the dynamic regions keeps the animation clean on the small TFT.
  tft.fillRect(12, 31, 136, 25, UI_BG);
  tft.setTextSize(3);
  tft.setTextColor(percent == 100 ? UI_TEAL : ST77XX_WHITE, UI_BG);
  tft.setCursor(13, 32);
  if (percent < 100) tft.printf("%02u", percent);
  else tft.print(F("OK"));
  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(62, 40);
  tft.print(percent == 100 ? F("SYSTEM READY") : F("INITIALIZING"));
  tft.setCursor(62, 49);
  tft.print(F("VARUNA CONTROL"));

  tft.fillRoundRect(12, 62, 136, 12, 2, UI_PANEL);
  uint16_t fillWidth = (uint16_t)map(percent, 0, 100, 0, 132);
  if (fillWidth > 0) {
    tft.fillRoundRect(14, 64, fillWidth, 8, 1,
                      percent == 100 ? UI_TEAL : UI_SKY);
  }
  int16_t markerX = 14 + map(percent, 0, 100, 0, 130);
  tft.fillRect(markerX, 61, 2, 14, UI_ORANGE);

  tft.fillRect(12, 79, 136, 13, UI_BG);
  tft.setTextColor(UI_ORANGE, UI_BG);
  tft.setCursor(12, 81);
  tft.print(F(">"));
  tft.setTextColor(ST77XX_WHITE, UI_BG);
  tft.setCursor(21, 81);
  tft.print(status);

  const uint8_t stagePercent[6] = {5, 20, 35, 68, 82, 94};
  const char *stageLabels[6] = {"LCD", "I2C", "ADC", "JOY", "RF", "LINK"};
  for (uint8_t i = 0; i < 6; ++i) {
    int16_t x = 13 + i * 26;
    bool complete = percent >= stagePercent[i];
    tft.fillRect(x, 101, 18, 2, complete ? UI_TEAL : UI_PANEL_EDGE);
    tft.setTextColor(complete ? UI_SKY : UI_MUTED, UI_BG);
    tft.setCursor(x, 106);
    tft.print(stageLabels[i]);
  }
}

static void drawInitializationSlide() {
  tft.fillScreen(UI_BG);

  // Diagnostic sequencer: a separate visual identity from the opening logo.
  tft.fillRect(0, 0, 160, 4, UI_TEAL);
  tft.fillRect(0, 4, 54, 2, UI_ORANGE);
  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(12, 13);
  tft.print(F("VARUNA / REMOTE CORE"));
  tft.setTextColor(UI_ORANGE, UI_BG);
  tft.setCursor(126, 13);
  tft.print(F("02"));
  tft.drawFastHLine(12, 24, 136, UI_PANEL_EDGE);
  tft.setTextColor(UI_SKY, UI_BG);
  tft.setCursor(12, 27);
  tft.print(F("HARDWARE STARTUP"));
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(102, 27);
  tft.print(F("LIVE CHECK"));

  tft.drawFastHLine(12, 94, 136, UI_PANEL_EDGE);
  tft.setCursor(12, 117);
  tft.print(F("DB/PVN  CONTROL OS  01"));
}

static void playBootAnimation() {
  tft.fillScreen(UI_BG);

  // SLIDE 1 — only the Varuna identity.
  // One centered V grows into place. It is drawn progressively without a
  // framebuffer, so boot performs no dynamic memory allocation.
  for (uint8_t frame = 1; frame <= 18; ++frame) {
    int16_t width = (frame * 31) / 18;
    int16_t height = (frame * 31) / 18;
    int16_t leftX = 80 - width;
    int16_t rightX = 80 + width;
    int16_t topY = 42 - height;

    tft.drawLine(leftX - 2, topY, 80, 44, UI_PANEL_EDGE);
    tft.drawLine(rightX + 2, topY, 80, 44, UI_PANEL_EDGE);
    for (uint8_t weight = 0; weight < 3; ++weight) {
      tft.drawLine(leftX + weight, topY, 80 + weight, 42, UI_SKY);
      tft.drawLine(rightX - weight, topY, 80 - weight, 42, UI_TEAL);
    }
    int16_t rule = (frame * 36) / 18;
    tft.drawFastHLine(80 - rule, 47, rule * 2, UI_PANEL_EDGE);
    tft.fillRect(77, 45, 7, 3, UI_ORANGE);
    ESP.wdtFeed();
    delay(22);
    yield();
  }

  // Clean centered wordmark with no secondary information on this slide.
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, UI_BG);
  tft.setCursor(44, 62);
  const char wordmark[] = "VARUNA";
  for (uint8_t i = 0; i < 6; ++i) {
    tft.print(wordmark[i]);
    delay(28);
  }
  tft.fillRoundRect(55, 86, 50, 3, 1, UI_TEAL);
  delay(650);

  // SLIDE 2 — company partnership and creators.
  tft.fillScreen(UI_BG);
  tft.fillRect(0, 0, 160, 4, UI_ORANGE);
  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(39, 12);
  tft.print(F("PROJECT PARTNERS"));

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, UI_BG);
  tft.setCursor(38, 27);
  tft.print(F("DOTBYTE"));
  tft.setTextSize(1);
  tft.setTextColor(UI_ORANGE, UI_BG);
  tft.setCursor(76, 47);
  tft.print('x');
  tft.setTextSize(2);
  tft.setTextColor(UI_TEAL, UI_BG);
  tft.setCursor(62, 56);
  tft.print(F("PVN"));
  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(59, 74);
  tft.print(F("COMPANY"));

  tft.drawFastHLine(18, 87, 124, UI_PANEL_EDGE);
  tft.setTextColor(UI_SKY, UI_BG);
  tft.setCursor(50, 93);
  tft.print(F("CREATED BY"));
  tft.setTextColor(ST77XX_WHITE, UI_BG);
  tft.setCursor(17, 106);
  tft.print(F("DEVAN"));
  tft.setCursor(104, 106);
  tft.print(F("ARUSH"));
  tft.setCursor(17, 118);
  tft.print(F("PARAMESWAR"));
  tft.setCursor(110, 118);
  tft.print(F("VINAY"));
  delay(900);
}

static void calibrateJoystickCenter() {
  // Keep the joystick released and centered while the startup screen is shown.
  int32_t totalX = 0;
  int32_t totalY = 0;
  constexpr uint8_t samples = 40;

  for (uint8_t i = 0; i < samples; ++i) {
    totalX += clampAdc(ads.readADC_SingleEnded(ADC_JOYSTICK_X));
    totalY += clampAdc(ads.readADC_SingleEnded(ADC_JOYSTICK_Y));
    if ((i % 5) == 0) {
      drawBootProgress(35 + ((uint16_t)i * 30 / samples),
                       F("CENTER JOYSTICK"));
    }
    delay(8);
  }

  joystickCenterX = totalX / samples;
  joystickCenterY = totalY / samples;
}

static void updateModeButton(uint8_t inputs) {
  bool rawPressed = (inputs & (1U << PCF_MODE_BUTTON_BIT)) == 0;
  uint32_t now = millis();

  if (rawPressed != lastRawButtonPressed) {
    lastRawButtonPressed = rawPressed;
    buttonChangedAt = now;
  }

  if ((now - buttonChangedAt) >= BUTTON_DEBOUNCE_MS &&
      rawPressed != debouncedButtonPressed) {
    debouncedButtonPressed = rawPressed;
    if (debouncedButtonPressed && appMode == APP_REMOTE) {
      lineFollowerMode = !lineFollowerMode;
    }
  }
}

static void updateSelectButton(uint8_t inputs) {
  bool rawPressed = (inputs & (1U << PCF_SELECT_BUTTON_BIT)) == 0;
  uint32_t now = millis();

  if (rawPressed != selectRawState) {
    selectRawState = rawPressed;
    selectChangedAt = now;
  }

  if ((now - selectChangedAt) >= BUTTON_DEBOUNCE_MS &&
      selectStableState != rawPressed) {
    selectStableState = rawPressed;
    if (selectStableState) {
      selectPressedAt = now;
      selectLongHandled = false;
    } else {
      if (!selectLongHandled) {
        selectClick = true;
        if (appMode == APP_REMOTE) {
          lineFollowerMode = true;
          pendingCalibrationFlag = wirelessCalibrationStep == 0
              ? COMMAND_CALIBRATE_WHITE : COMMAND_CALIBRATE_BLACK;
          wirelessCalibrationStep = (wirelessCalibrationStep + 1) % 2;
        }
      }
    }
  }

  if (selectStableState && !selectLongHandled &&
      (now - selectPressedAt) >= 5000) {
    selectLongHandled = true;
    if (appMode == APP_REMOTE) {
      easterRequested = true;
    } else if (appMode == APP_DOTFRAME) {
      ESP.restart();
    } else {
      easterExitRequested = true;
    }
  }
}

static void readControls() {
  uint8_t inputs = readPcf8574();
  updateModeButton(inputs);
  updateSelectButton(inputs);

  command.magic = COMMAND_MAGIC;
  command.version = PROTOCOL_VERSION;
  command.mode = lineFollowerMode ? MODE_LINE_FOLLOWER : MODE_MANUAL;
  command.speedPwm = readSpeedPwm();
  command.flags = pendingCalibrationFlag;
  command.steering = normalizeJoystick(
      ads.readADC_SingleEnded(ADC_JOYSTICK_X), joystickCenterX,
      INVERT_JOYSTICK_X);
  command.drive = normalizeJoystick(
      ads.readADC_SingleEnded(ADC_JOYSTICK_Y), joystickCenterY,
      INVERT_JOYSTICK_Y);
  uiDrive = command.drive;
  uiSteering = command.steering;

  if (appMode != APP_REMOTE) {
    // Redundant safety state: even a receiver that checks neutral commands
    // before the explicit E-stop flag sees a completely stopped vehicle.
    command.mode = MODE_MANUAL;
    command.speedPwm = 0;
    command.flags = COMMAND_ESTOP;
    command.drive = 0;
    command.steering = 0;
  }

  // Joystick commands are ignored by the vehicle in autonomous mode, but are
  // still transmitted to make switching back to manual mode instantaneous.
}

static bool validTelemetry(const TelemetryPacket &packet) {
  return packet.magic == TELEMETRY_MAGIC &&
         packet.version == PROTOCOL_VERSION;
}

static void sendCommand() {
  command.sequence++;
  packetsSent++;

  bool acknowledged = radio.write(&command, sizeof(command));
  if (acknowledged) packetsAcknowledged++;
  if (acknowledged) pendingCalibrationFlag = 0;

  // The ESP32 vehicle should preload its latest telemetry with
  // radio.writeAckPayload(). It is returned in the command acknowledgement.
  if (radio.available()) {
    uint8_t payloadSize = radio.getDynamicPayloadSize();
    if (payloadSize == sizeof(TelemetryPacket)) {
      TelemetryPacket incoming;
      radio.read(&incoming, sizeof(incoming));
      if (validTelemetry(incoming)) {
        telemetry = incoming;
        lastTelemetryAt = millis();
      }
    } else if (payloadSize > 0 && payloadSize <= 32) {
      uint8_t discard[32];
      radio.read(discard, payloadSize);
    } else {
      radio.flush_rx();
    }
  }
}

static void drawCard(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRoundRect(x, y, w, h, 4, UI_PANEL);
  tft.drawRoundRect(x, y, w, h, 4, UI_PANEL_EDGE);
}

static void drawDashboardFrame() {
  tft.fillScreen(UI_BG);

  // Premium three-zone dashboard: drive, input, and vehicle state.
  tft.fillRect(0, 0, 160, 22, UI_PANEL);
  tft.drawFastHLine(0, 21, 160, UI_TEAL);
  drawCard(3, 26, 72, 58);
  drawCard(79, 26, 78, 58);
  drawCard(3, 88, 154, 37);

  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_PANEL);
  tft.setCursor(9, 31);
  tft.print(F("DRIVE LIMIT"));
  tft.setCursor(85, 31);
  tft.print(F("JOYSTICK"));
  tft.setCursor(9, 92);
  tft.print(F("VEHICLE STATUS"));
  tft.setCursor(128, 92);
  tft.print(F("IMU"));

  dashboardFrameDrawn = true;
}

static void drawLinkIndicator(bool linked) {
  // Small antenna bars at the top-right.
  tft.fillRect(143, 3, 14, 13, UI_PANEL);
  uint16_t color = linked ? UI_TEAL : UI_RED;
  for (uint8_t i = 0; i < 4; ++i) {
    uint8_t height = 3 + i * 3;
    tft.fillRect(143 + i * 3, 16 - height, 2, height,
                 linked ? color : UI_PANEL_EDGE);
  }
  if (!linked) {
    tft.drawLine(143, 4, 155, 16, UI_RED);
  }
}

static void drawBatteryGauge(bool linked) {
  const int16_t x = 9;
  const int16_t y = 116;
  const int16_t w = 56;
  const int16_t h = 6;
  tft.fillRect(x - 1, y - 1, w + 4, h + 2, UI_PANEL);
  tft.drawRoundRect(x, y, w, h, 2, UI_MUTED);
  tft.fillRect(x + w, y + 2, 2, 2, UI_MUTED);

  if (!linked) return;

  int percentage = map(constrain(telemetry.batteryMv, BATTERY_EMPTY_MV,
                                 BATTERY_FULL_MV),
                       BATTERY_EMPTY_MV, BATTERY_FULL_MV, 0, 100);
  int fillWidth = map(percentage, 0, 100, 0, w - 4);
  uint16_t color = percentage < 20 ? UI_RED
                       : percentage < 50 ? UI_ORANGE
                                         : UI_TEAL;
  tft.fillRoundRect(x + 2, y + 2, fillWidth, h - 4, 1, color);
}

static void drawJoystickWidget() {
  const int16_t cx = 15;
  const int16_t cy = 15;
  const int16_t radius = 13;
  joystickCanvas.fillScreen(UI_PANEL);
  joystickCanvas.drawCircle(cx, cy, radius, UI_PANEL_EDGE);
  joystickCanvas.drawFastHLine(cx - radius + 3, cy, radius * 2 - 5,
                               UI_PANEL_EDGE);
  joystickCanvas.drawFastVLine(cx, cy - radius + 3, radius * 2 - 5,
                               UI_PANEL_EDGE);

  int16_t dotX = cx + map(command.steering, -1000, 1000, -9, 9);
  int16_t dotY = cy - map(command.drive, -1000, 1000, -9, 9);
  joystickCanvas.fillCircle(dotX, dotY, 3, UI_SKY);
  joystickCanvas.drawPixel(dotX, dotY, ST77XX_WHITE);
  tft.drawRGBBitmap(123, 39, joystickCanvas.getBuffer(), 31, 31);
}

static void drawHorizonSphere(bool telemetryAvailable) {
  constexpr int16_t center = 13;
  constexpr int16_t radius = 11;
  horizonCanvas.fillScreen(UI_PANEL);

  float rollDegrees = telemetryAvailable
      ? constrain(telemetry.rollCdeg / 100.0f, -45.0f, 45.0f)
      : 0.0f;
  float pitchDegrees = telemetryAvailable
      ? constrain(telemetry.pitchCdeg / 100.0f, -30.0f, 30.0f)
      : 0.0f;
  float slope = tanf(rollDegrees * DEG_TO_RAD);
  float pitchOffset = map((int)pitchDegrees, -30, 30, -8, 8);

  // Rasterize a clipped artificial horizon: blue sky above the calculated
  // horizon and red ground below it.
  for (int16_t y = -radius; y <= radius; ++y) {
    for (int16_t x = -radius; x <= radius; ++x) {
      if ((x * x + y * y) <= radius * radius) {
        float horizonY = pitchOffset + slope * x;
        uint16_t color = y < horizonY ? HORIZON_BLUE : HORIZON_RED;
        horizonCanvas.drawPixel(center + x, center + y, color);
      }
    }
  }

  // Horizon and vehicle reference marks.
  for (int16_t x = -9; x <= 9; ++x) {
    int16_t y = roundf(pitchOffset + slope * x);
    if ((x * x + y * y) <= radius * radius) {
      horizonCanvas.drawPixel(center + x, center + y, ST77XX_WHITE);
    }
  }
  horizonCanvas.drawFastHLine(center - 7, center, 5, UI_ORANGE);
  horizonCanvas.drawFastHLine(center + 3, center, 5, UI_ORANGE);
  horizonCanvas.drawPixel(center, center, UI_ORANGE);
  horizonCanvas.drawCircle(center, center, radius,
                           telemetryAvailable ? ST77XX_WHITE : UI_MUTED);

  // Transfer the complete sphere at once to prevent animation flicker.
  tft.drawRGBBitmap(125, 96, horizonCanvas.getBuffer(), 27, 27);
}

static void drawScreen() {
  if (!dashboardFrameDrawn) drawDashboardFrame();

  bool linked = (millis() - lastTelemetryAt) < LINK_TIMEOUT_MS;
  bool gpsFix = linked && (telemetry.flags & TELEMETRY_GPS_FIX);

  // Cached values provide dirty-region rendering. Large TFT regions are no
  // longer erased and repainted on every pass, which prevents visible flashing.
  static int8_t previousMode = -1;
  static int8_t previousLinked = -1;
  static int16_t previousSpeed = -1;
  static int16_t previousDrive = -32768;
  static int16_t previousSteering = -32768;
  static uint16_t previousBattery = 0xFFFF;
  static int8_t previousGpsFix = -1;
  static uint8_t previousSatellites = 0xFF;
  static int8_t previousMpuAvailable = -1;
  static int16_t previousRoll = -32768;
  static int16_t previousPitch = -32768;

  if (forceDashboardRefresh) {
    previousMode = -1;
    previousLinked = -1;
    previousSpeed = -1;
    previousDrive = -32768;
    previousSteering = -32768;
    previousBattery = 0xFFFF;
    previousGpsFix = -1;
    previousSatellites = 0xFF;
    previousMpuAvailable = -1;
    previousRoll = -32768;
    previousPitch = -32768;
    forceDashboardRefresh = false;
  }

  tft.setTextSize(1);

  // Header: brand, mode pill, and radio strength.
  if (previousMode != (int8_t)lineFollowerMode) {
    tft.fillRect(0, 0, 140, 21, UI_PANEL);
    tft.setCursor(6, 7);
    tft.setTextColor(ST77XX_WHITE, UI_PANEL);
    tft.print(F("VARUNA"));

    uint16_t modeColor = lineFollowerMode ? UI_ORANGE : UI_TEAL;
    tft.fillRoundRect(54, 4, 82, 14, 7, modeColor);
    tft.setTextColor(UI_BG, modeColor);
    tft.setCursor(lineFollowerMode ? 59 : 74, 7);
    tft.print(lineFollowerMode ? F("LINE FOLLOW") : F("MANUAL"));
    previousMode = lineFollowerMode;
  }
  if (previousLinked != (int8_t)linked) {
    drawLinkIndicator(linked);
  }

  // Large speed number.
  if (previousSpeed != command.speedPwm) {
    tft.fillRect(8, 42, 62, 35, UI_PANEL);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(3);
    tft.setCursor(command.speedPwm < 100 ? 25 : 12, 44);
    tft.printf("%u", command.speedPwm);
    tft.setTextSize(1);
    tft.setTextColor(UI_MUTED, UI_PANEL);
    tft.setCursor(50, 70);
    tft.print(F("PWM"));

    // Slider-like visual makes the 60-225 range readable at a glance.
    tft.fillRoundRect(10, 79, 58, 3, 1, UI_PANEL_EDGE);
    int16_t speedWidth = map(command.speedPwm, 60, 225, 2, 58);
    tft.fillRoundRect(10, 79, speedWidth, 3, 1,
                      command.speedPwm > 190 ? UI_ORANGE : UI_TEAL);
    previousSpeed = command.speedPwm;
  }

  // Joystick numeric values and visual position.
  if (abs(command.drive - previousDrive) >= 10 ||
      abs(command.steering - previousSteering) >= 10) {
    tft.fillRect(84, 41, 35, 39, UI_PANEL);
    tft.setTextColor(UI_MUTED, UI_PANEL);
    tft.setCursor(85, 42);
    tft.print(F("FWD"));
    tft.setTextColor(ST77XX_WHITE, UI_PANEL);
    tft.setCursor(85, 51);
    tft.printf("%+4d", command.drive / 10);
    tft.setTextColor(UI_MUTED, UI_PANEL);
    tft.setCursor(85, 63);
    tft.print(F("TURN"));
    tft.setTextColor(ST77XX_WHITE, UI_PANEL);
    tft.setCursor(85, 72);
    tft.printf("%+4d", command.steering / 10);
    drawJoystickWidget();
    previousDrive = command.drive;
    previousSteering = command.steering;
  }

  // Vehicle battery and GPS status.
  if (previousLinked != (int8_t)linked ||
      (linked && abs((int32_t)telemetry.batteryMv - previousBattery) >= 10)) {
    tft.fillRect(8, 101, 60, 14, UI_PANEL);
    tft.setCursor(9, 103);
    if (linked) {
      tft.setTextColor(ST77XX_WHITE, UI_PANEL);
      tft.printf("%u.%02u V", telemetry.batteryMv / 1000,
                 (telemetry.batteryMv % 1000) / 10);
    } else {
      tft.setTextColor(UI_MUTED, UI_PANEL);
      tft.print(F("--.-- V"));
    }
    drawBatteryGauge(linked);
    previousBattery = telemetry.batteryMv;
  }

  if (previousGpsFix != (int8_t)gpsFix ||
      previousSatellites != telemetry.satellites) {
    tft.fillRect(72, 99, 49, 23, UI_PANEL);
    tft.setCursor(76, 102);
    if (gpsFix) {
      tft.setTextColor(UI_TEAL, UI_PANEL);
      tft.print(F("GPS"));
      tft.setCursor(76, 113);
      tft.setTextColor(ST77XX_WHITE, UI_PANEL);
      tft.printf("FIX %02u", telemetry.satellites);
    } else {
      tft.setTextColor(UI_ORANGE, UI_PANEL);
      tft.print(F("GPS"));
      tft.setCursor(76, 113);
      tft.setTextColor(UI_MUTED, UI_PANEL);
      tft.print(F("SEARCH"));
    }
    previousGpsFix = gpsFix;
    previousSatellites = telemetry.satellites;
  }

  // Artificial horizon replaces numeric roll/pitch and ON/OFF text.
  bool mpuTelemetryAvailable = linked &&
      (telemetry.flags & TELEMETRY_MPU_OK);
  bool horizonChanged = previousMpuAvailable != (int8_t)mpuTelemetryAvailable ||
      abs((int32_t)telemetry.rollCdeg - previousRoll) >= 50 ||
      abs((int32_t)telemetry.pitchCdeg - previousPitch) >= 50;
  if (horizonChanged) {
    tft.fillRect(124, 95, 29, 29, UI_PANEL);
    drawHorizonSphere(mpuTelemetryAvailable);
    previousMpuAvailable = mpuTelemetryAvailable;
    previousRoll = telemetry.rollCdeg;
    previousPitch = telemetry.pitchCdeg;
  }

  previousLinked = linked;
}

static const char *EASTER_ITEMS[] = {
  "STAR CATCH", "REACTION", "NEON SNAKE",
  "ORBIT DODGE", "DOTFRAME", "EXIT"
};
constexpr uint8_t EASTER_ITEM_COUNT =
    sizeof(EASTER_ITEMS) / sizeof(EASTER_ITEMS[0]);

static void drawEasterMenuItem(uint8_t index) {
  uint8_t row = index / 2;
  uint8_t column = index % 2;
  int16_t x = 5 + column * 78;
  int16_t y = 31 + row * 28;
  bool selected = index == easterMenuIndex;
  uint16_t accent = index == EASTER_ITEM_COUNT - 1 ? UI_ORANGE : UI_TEAL;

  tft.fillRoundRect(x, y, 72, 24, 5, selected ? accent : UI_PANEL);
  tft.drawRoundRect(x, y, 72, 24, 5,
                    selected ? ST77XX_WHITE : UI_PANEL_EDGE);
  tft.setTextSize(1);
  tft.setTextColor(selected ? UI_BG : UI_MUTED,
                   selected ? accent : UI_PANEL);
  tft.setCursor(x + 6, y + 4);
  tft.printf("0%u", index + 1);
  tft.setTextColor(selected ? UI_BG : ST77XX_WHITE,
                   selected ? accent : UI_PANEL);
  tft.setCursor(x + 4, y + 14);
  tft.print(EASTER_ITEMS[index]);
}

static void drawEasterMenu() {
  tft.fillScreen(UI_BG);
  tft.fillRect(0, 0, 160, 4, UI_TEAL);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, UI_BG);
  tft.setCursor(7, 10);
  tft.print(F("VARUNA // EASTER EGGS"));
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(7, 21);
  tft.print(F("CHOOSE AN EXPERIENCE"));

  for (uint8_t i = 0; i < EASTER_ITEM_COUNT; ++i) drawEasterMenuItem(i);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(7, 119);
  tft.print(F("MOVE: JOY  OPEN: SELECT"));
}

static void showGameHeader(const __FlashStringHelper *title) {
  tft.fillScreen(UI_BG);
  tft.fillRect(0, 0, 160, 18, UI_PANEL);
  tft.drawFastHLine(0, 17, 160, UI_TEAL);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, UI_PANEL);
  tft.setCursor(6, 5);
  tft.print(title);
  tft.setTextColor(UI_MUTED, UI_PANEL);
  tft.setCursor(102, 5);
  tft.print(F("HOLD=EXIT"));
}

static int16_t starPlayerX = 80;
static int16_t fallingStarX = 40;
static int16_t fallingStarY = 25;
static uint16_t starScore = 0;
static uint8_t starMisses = 0;
static uint32_t starLastStep = 0;
static bool starPaused = false;

static void drawStarScore() {
  tft.fillRect(0, 19, 160, 16, UI_BG);
  tft.setTextColor(UI_SKY, UI_BG);
  tft.setCursor(6, 22);
  tft.printf("SCORE %u", starScore);
  tft.setTextColor(UI_ORANGE, UI_BG);
  tft.setCursor(111, 22);
  tft.printf("MISS %u", starMisses);
}

static void beginStarCatch() {
  starPlayerX = 80;
  fallingStarX = random(10, 150);
  fallingStarY = 40;
  starScore = 0;
  starMisses = 0;
  starPaused = false;
  starLastStep = millis();
  showGameHeader(F("STAR CATCH"));
  drawStarScore();
  tft.drawFastHLine(5, 123, 150, UI_PANEL_EDGE);
  tft.fillTriangle(fallingStarX, fallingStarY - 4,
                   fallingStarX - 4, fallingStarY + 4,
                   fallingStarX + 4, fallingStarY + 4, ST77XX_YELLOW);
  tft.fillRoundRect(starPlayerX - 11, 116, 22, 5, 2, UI_TEAL);
}

static void updateStarCatch() {
  uint32_t now = millis();

  if (selectClick) {
    selectClick = false;
    starPaused = !starPaused;
    tft.fillRect(56, 22, 48, 9, UI_BG);
    if (starPaused) {
      tft.setTextColor(ST77XX_WHITE, UI_BG);
      tft.setCursor(62, 22);
      tft.print(F("PAUSED"));
    } else {
      drawStarScore();
      starLastStep = now;
    }
  }
  if (starPaused) return;
  if (now - starLastStep < 70) return;
  starLastStep = now;

  int16_t oldPlayerX = starPlayerX;
  int16_t oldStarX = fallingStarX;
  int16_t oldStarY = fallingStarY;

  // Erase only the previous sprites, preserving the static playfield.
  tft.fillRect(oldStarX - 5, oldStarY - 5, 11, 11, UI_BG);
  tft.fillRect(oldPlayerX - 12, 115, 24, 7, UI_BG);

  starPlayerX += map(uiSteering, -1000, 1000, -5, 5);
  starPlayerX = constrain(starPlayerX, 12, 148);
  fallingStarY += 4 + min((uint16_t)4, (uint16_t)(starScore / 5));

  if (fallingStarY >= 111) {
    if (abs(fallingStarX - starPlayerX) <= 14) {
      starScore++;
      tone(BUZZER_PIN, 1500, 35);
    } else {
      starMisses++;
    }
    fallingStarX = random(10, 150);
    fallingStarY = 40;
    drawStarScore();
  }

  tft.fillTriangle(fallingStarX, fallingStarY - 4,
                   fallingStarX - 4, fallingStarY + 4,
                   fallingStarX + 4, fallingStarY + 4, ST77XX_YELLOW);
  tft.fillRoundRect(starPlayerX - 11, 116, 22, 5, 2, UI_TEAL);
}

enum ReactionState : uint8_t { REACTION_IDLE, REACTION_WAIT, REACTION_GO,
                               REACTION_RESULT };
static ReactionState reactionState = REACTION_IDLE;
static uint32_t reactionDeadline = 0;
static uint32_t reactionGoAt = 0;

static void drawReactionState(const __FlashStringHelper *headline,
                              uint16_t color,
                              const __FlashStringHelper *subline) {
  tft.fillRect(0, 19, 160, 109, UI_BG);
  tft.fillCircle(80, 65, 27, color);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, color);
  tft.setCursor(48, 58);
  tft.print(headline);
  tft.setTextSize(1);
  tft.setTextColor(UI_MUTED, UI_BG);
  tft.setCursor(20, 101);
  tft.print(subline);
}

static void beginReaction() {
  reactionState = REACTION_IDLE;
  showGameHeader(F("REACTION RUSH"));
  drawReactionState(F("READY"), UI_PANEL_EDGE, F("Press SELECT to arm"));
}

static void updateReaction() {
  uint32_t now = millis();
  if (reactionState == REACTION_WAIT && now >= reactionDeadline) {
    reactionState = REACTION_GO;
    reactionGoAt = now;
    drawReactionState(F("GO!"), UI_TEAL, F("PRESS SELECT NOW"));
    tone(BUZZER_PIN, 1800, 45);
  }

  if (!selectClick) return;
  selectClick = false;
  if (reactionState == REACTION_IDLE || reactionState == REACTION_RESULT) {
    reactionState = REACTION_WAIT;
    reactionDeadline = now + random(1200, 3500);
    drawReactionState(F("WAIT"), UI_ORANGE, F("Wait for green..."));
  } else if (reactionState == REACTION_WAIT) {
    reactionState = REACTION_RESULT;
    drawReactionState(F("EARLY"), UI_RED, F("Press SELECT to retry"));
  } else if (reactionState == REACTION_GO) {
    uint32_t reactionMs = now - reactionGoAt;
    reactionState = REACTION_RESULT;
    tft.fillRect(0, 19, 160, 109, UI_BG);
    tft.setTextSize(2);
    tft.setTextColor(UI_TEAL, UI_BG);
    tft.setCursor(48, 48);
    tft.printf("%lu", (unsigned long)reactionMs);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, UI_BG);
    tft.setCursor(68, 68);
    tft.print(F("ms"));
    tft.setTextColor(UI_MUTED, UI_BG);
    tft.setCursor(20, 101);
    tft.print(F("Press SELECT to retry"));
  }
}

constexpr uint8_t SNAKE_COLS = 18;
constexpr uint8_t SNAKE_ROWS = 12;
constexpr uint8_t SNAKE_MAX_LENGTH = 50;
static uint8_t snakeX[SNAKE_MAX_LENGTH];
static uint8_t snakeY[SNAKE_MAX_LENGTH];
static uint8_t snakeLength = 0;
static int8_t snakeDx = 1;
static int8_t snakeDy = 0;
static uint8_t snakeFoodX = 3;
static uint8_t snakeFoodY = 3;
static bool snakeGameOver = false;
static bool snakePaused = false;
static uint32_t snakeLastStep = 0;

static void drawSnakeCell(uint8_t x, uint8_t y, uint16_t color) {
  tft.fillRect(18 + x * 7, 35 + y * 7, 6, 6, color);
}

static bool snakeOccupies(uint8_t x, uint8_t y) {
  for (uint8_t i = 0; i < snakeLength; ++i) {
    if (snakeX[i] == x && snakeY[i] == y) return true;
  }
  return false;
}

static void placeSnakeFood() {
  for (uint8_t attempt = 0; attempt < 80; ++attempt) {
    uint8_t x = random(SNAKE_COLS);
    uint8_t y = random(SNAKE_ROWS);
    if (!snakeOccupies(x, y)) {
      snakeFoodX = x;
      snakeFoodY = y;
      break;
    }
  }
  drawSnakeCell(snakeFoodX, snakeFoodY, UI_ORANGE);
}

static void beginNeonSnake() {
  snakeLength = 3;
  snakeX[0] = 9; snakeY[0] = 6;
  snakeX[1] = 8; snakeY[1] = 6;
  snakeX[2] = 7; snakeY[2] = 6;
  snakeDx = 1;
  snakeDy = 0;
  snakeGameOver = false;
  snakePaused = false;
  snakeLastStep = millis();
  showGameHeader(F("NEON SNAKE"));
  tft.drawRect(16, 33, 128, 88, UI_PANEL_EDGE);
  for (uint8_t i = 0; i < snakeLength; ++i) {
    drawSnakeCell(snakeX[i], snakeY[i], i == 0 ? UI_SKY : UI_TEAL);
  }
  placeSnakeFood();
}

static void updateNeonSnake() {
  uint32_t now = millis();
  if (selectClick) {
    selectClick = false;
    if (snakeGameOver) {
      beginNeonSnake();
      return;
    }
    snakePaused = !snakePaused;
    tft.fillRect(55, 21, 55, 9, UI_BG);
    if (snakePaused) {
      tft.setTextColor(ST77XX_WHITE, UI_BG);
      tft.setCursor(62, 22);
      tft.print(F("PAUSED"));
    }
  }
  if (snakePaused || snakeGameOver) return;

  if (abs(uiSteering) > abs(uiDrive)) {
    if (uiSteering > 500 && snakeDx != -1) { snakeDx = 1; snakeDy = 0; }
    if (uiSteering < -500 && snakeDx != 1) { snakeDx = -1; snakeDy = 0; }
  } else {
    if (uiDrive > 500 && snakeDy != 1) { snakeDx = 0; snakeDy = -1; }
    if (uiDrive < -500 && snakeDy != -1) { snakeDx = 0; snakeDy = 1; }
  }

  uint16_t interval = max((int)75, 190 - (int)snakeLength * 2);
  if (now - snakeLastStep < interval) return;
  snakeLastStep = now;

  int16_t newX = snakeX[0] + snakeDx;
  int16_t newY = snakeY[0] + snakeDy;
  if (newX < 0 || newX >= SNAKE_COLS || newY < 0 || newY >= SNAKE_ROWS ||
      snakeOccupies(newX, newY)) {
    snakeGameOver = true;
    tone(BUZZER_PIN, 260, 180);
    tft.fillRoundRect(35, 59, 90, 29, 5, UI_PANEL);
    tft.drawRoundRect(35, 59, 90, 29, 5, UI_RED);
    tft.setTextColor(UI_RED, UI_PANEL);
    tft.setCursor(53, 65);
    tft.print(F("GAME OVER"));
    tft.setTextColor(UI_MUTED, UI_PANEL);
    tft.setCursor(44, 77);
    tft.print(F("SELECT=RETRY"));
    return;
  }

  bool ate = newX == snakeFoodX && newY == snakeFoodY;
  if (!ate) {
    drawSnakeCell(snakeX[snakeLength - 1], snakeY[snakeLength - 1], UI_BG);
  } else if (snakeLength < SNAKE_MAX_LENGTH) {
    snakeLength++;
    tone(BUZZER_PIN, 1450, 30);
  }
  for (int16_t i = snakeLength - 1; i > 0; --i) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  drawSnakeCell(snakeX[0], snakeY[0], UI_TEAL);
  snakeX[0] = newX;
  snakeY[0] = newY;
  drawSnakeCell(snakeX[0], snakeY[0], UI_SKY);
  if (ate) placeSnakeFood();
}

static uint8_t dodgeLane = 1;
static uint8_t obstacleLane = 0;
static int16_t obstacleY = 35;
static uint16_t dodgeScore = 0;
static uint8_t dodgeLives = 3;
static bool dodgeMoveLatched = false;
static bool dodgePaused = false;
static uint32_t dodgeLastStep = 0;

static int16_t laneX(uint8_t lane) { return 35 + lane * 45; }

static void drawDodgePlayer(uint16_t color) {
  int16_t x = laneX(dodgeLane);
  tft.fillTriangle(x, 108, x - 9, 121, x + 9, 121, color);
}

static void drawDodgeScore() {
  tft.fillRect(0, 19, 160, 14, UI_BG);
  tft.setTextColor(UI_SKY, UI_BG);
  tft.setCursor(7, 22);
  tft.printf("SCORE %u", dodgeScore);
  tft.setTextColor(UI_ORANGE, UI_BG);
  tft.setCursor(116, 22);
  tft.printf("L%u", dodgeLives);
}

static void beginOrbitDodge() {
  dodgeLane = 1;
  obstacleLane = random(3);
  obstacleY = 36;
  dodgeScore = 0;
  dodgeLives = 3;
  dodgeMoveLatched = false;
  dodgePaused = false;
  dodgeLastStep = millis();
  showGameHeader(F("ORBIT DODGE"));
  drawDodgeScore();
  tft.drawFastVLine(57, 34, 90, UI_PANEL_EDGE);
  tft.drawFastVLine(102, 34, 90, UI_PANEL_EDGE);
  drawDodgePlayer(UI_TEAL);
  tft.fillRoundRect(laneX(obstacleLane) - 7, obstacleY, 14, 9, 3, UI_RED);
}

static void updateOrbitDodge() {
  uint32_t now = millis();
  if (selectClick) {
    selectClick = false;
    if (dodgeLives == 0) {
      beginOrbitDodge();
      return;
    }
    dodgePaused = !dodgePaused;
  }
  if (dodgePaused || dodgeLives == 0) return;

  if (!dodgeMoveLatched && abs(uiSteering) > 500) {
    drawDodgePlayer(UI_BG);
    if (uiSteering > 0 && dodgeLane < 2) dodgeLane++;
    if (uiSteering < 0 && dodgeLane > 0) dodgeLane--;
    drawDodgePlayer(UI_TEAL);
    dodgeMoveLatched = true;
  } else if (abs(uiSteering) < 220) {
    dodgeMoveLatched = false;
  }

  if (now - dodgeLastStep < 75) return;
  dodgeLastStep = now;
  tft.fillRoundRect(laneX(obstacleLane) - 8, obstacleY - 1, 16, 11, 3,
                    UI_BG);
  obstacleY += 5 + min((uint16_t)4, (uint16_t)(dodgeScore / 8));
  if (obstacleY >= 108) {
    if (obstacleLane == dodgeLane) {
      dodgeLives--;
      tone(BUZZER_PIN, 300, 100);
    } else {
      dodgeScore++;
    }
    obstacleLane = random(3);
    obstacleY = 36;
    drawDodgeScore();
    if (dodgeLives == 0) {
      tft.fillRoundRect(34, 58, 92, 31, 5, UI_PANEL);
      tft.drawRoundRect(34, 58, 92, 31, 5, UI_RED);
      tft.setTextColor(UI_RED, UI_PANEL);
      tft.setCursor(52, 64);
      tft.print(F("GAME OVER"));
      tft.setTextColor(UI_MUTED, UI_PANEL);
      tft.setCursor(43, 77);
      tft.print(F("SELECT=RETRY"));
      return;
    }
  }
  tft.fillRoundRect(laneX(obstacleLane) - 7, obstacleY, 14, 9, 3, UI_RED);
}

static void enterEasterMenu() {
  appMode = APP_EASTER_MENU;
  easterMenuIndex = 0;
  dashboardFrameDrawn = false;
  selectClick = false;
  tone(BUZZER_PIN, 880, 70);
  drawEasterMenu();
}

static void returnToRemote() {
  appMode = APP_REMOTE;
  easterExitRequested = false;
  selectClick = false;
  dashboardFrameDrawn = false;
  forceDashboardRefresh = true;
  lastScreenAt = millis() - SCREEN_PERIOD_MS;
  tft.fillScreen(UI_BG);
}

static void showFatal(const __FlashStringHelper *message);

static void updateEasterApps() {
  static bool navigationLatched = false;

  if (easterExitRequested && appMode != APP_DOTFRAME) {
    returnToRemote();
    return;
  }

  if (appMode == APP_EASTER_MENU) {
    if (!navigationLatched &&
        (abs(uiDrive) > 550 || abs(uiSteering) > 550)) {
      uint8_t previousIndex = easterMenuIndex;
      uint8_t row = easterMenuIndex / 2;
      uint8_t column = easterMenuIndex % 2;
      if (abs(uiSteering) > abs(uiDrive)) {
        column = uiSteering > 0 ? 1 : 0;
      } else {
        row = uiDrive > 0 ? (row + 2) % 3 : (row + 1) % 3;
      }
      easterMenuIndex = row * 2 + column;
      navigationLatched = true;
      tone(BUZZER_PIN, 1050, 20);
      drawEasterMenuItem(previousIndex);
      drawEasterMenuItem(easterMenuIndex);
    } else if (abs(uiDrive) < 220 && abs(uiSteering) < 220) {
      navigationLatched = false;
    }

    if (selectClick) {
      selectClick = false;
      if (easterMenuIndex == 0) {
        appMode = APP_STAR_CATCH;
        beginStarCatch();
      } else if (easterMenuIndex == 1) {
        appMode = APP_REACTION;
        beginReaction();
      } else if (easterMenuIndex == 2) {
        appMode = APP_NEON_SNAKE;
        beginNeonSnake();
      } else if (easterMenuIndex == 3) {
        appMode = APP_ORBIT_DODGE;
        beginOrbitDodge();
      } else if (easterMenuIndex == 4) {
        appMode = APP_DOTFRAME_ARMING;
        dotFrameArmStarted = millis();
        tft.fillScreen(UI_BG);
        tft.setTextColor(UI_RED, UI_BG);
        tft.setTextSize(2);
        tft.setCursor(21, 37);
        tft.print(F("SAFE STOP"));
        tft.setTextSize(1);
        tft.setTextColor(UI_MUTED, UI_BG);
        tft.setCursor(18, 67);
        tft.print(F("Stopping vehicle before"));
        tft.setCursor(34, 79);
        tft.print(F("starting Wi-Fi..."));
      } else {
        returnToRemote();
      }
    }
  } else if (appMode == APP_STAR_CATCH) {
    updateStarCatch();
  } else if (appMode == APP_REACTION) {
    updateReaction();
  } else if (appMode == APP_NEON_SNAKE) {
    updateNeonSnake();
  } else if (appMode == APP_ORBIT_DODGE) {
    updateOrbitDodge();
  } else if (appMode == APP_DOTFRAME_ARMING &&
             millis() - dotFrameArmStarted >= 1000) {
    radio.powerDown();
    if (DotFrameMode::begin(tft)) {
      appMode = APP_DOTFRAME;
    } else {
      showFatal(F("DotFrame Wi-Fi failed"));
    }
  }
}

static void showFatal(const __FlashStringHelper *message) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(4, 20);
  tft.println(F("ERROR"));
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 50);
  tft.println(message);
  while (true) delay(100);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(NRF_CSN_PIN, OUTPUT);
  digitalWrite(NRF_CSN_PIN, HIGH);
  pinMode(NRF_CE_PIN, OUTPUT);
  digitalWrite(NRF_CE_PIN, LOW);

  // This is a dedicated nRF24 remote. Keeping the ESP8266 Wi-Fi radio and its
  // background interrupt/power-management work disabled improves stability,
  // reduces current draw, and avoids unnecessary 2.4 GHz interference.
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);

  SPI.begin(); // GPIO14=SCK, GPIO12=MISO, GPIO13=MOSI on ESP8266
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // 160x128 landscape
  tft.setTextWrap(false);
  playBootAnimation();
  // SLIDE 3 — actual initialization progress.
  drawInitializationSlide();
  drawBootProgress(5, F("DISPLAY ONLINE"));

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  drawBootProgress(20, F("I2C BUS ONLINE"));

  if (!beginPcf8574()) {
    showFatal(F("PCF8574 not found"));
  }
  Serial.println(F("[OK] PCF8574 detected at 0x27"));
  drawBootProgress(27, F("BUTTON PANEL ONLINE"));

  Serial.println(F("[BOOT] Starting ADS1115 detection"));
  if (!ads.begin(0x48, &Wire)) {
    showFatal(F("ADS1115 not found"));
  }
  Serial.println(F("[OK] ADS1115 detected at 0x48"));
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_860SPS);
  drawBootProgress(35, F("ADS1115 CONNECTED"));
  Serial.println(F("[BOOT] Starting joystick calibration"));
  calibrateJoystickCenter();
  Serial.println(F("[OK] Joystick calibration complete"));
  drawBootProgress(68, F("CONTROLS CALIBRATED"));

  Serial.println(F("[BOOT] Starting nRF24 detection"));
  bool radioReady = false;
  for (uint8_t attempt = 1; attempt <= 3 && !radioReady; ++attempt) {
    radioReady = radio.begin();
    if (radioReady && !radio.isChipConnected()) {
      radioReady = false;
    }
    if (!radioReady) {
      Serial.print(F("[WARN] nRF24 init attempt "));
      Serial.print(attempt);
      Serial.println(F(" failed"));
      digitalWrite(NRF_CE_PIN, LOW);
      digitalWrite(NRF_CSN_PIN, HIGH);
      delay(150);
    }
  }
  if (!radioReady) {
    showFatal(F("nRF24 not found"));
  }
  Serial.println(F("[OK] nRF24 detected"));
  drawBootProgress(82, F("NRF24 CONNECTED"));
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.setAddressWidth(5);
  radio.setRetries(5, 15);
  radio.setCRCLength(RF24_CRC_16);
  // Match the vehicle on 2476 MHz. The robust 250 kbps rate is preferable
  // for a safety-critical moving vehicle and inexpensive nRF24 modules.
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.openWritingPipe(RADIO_ADDRESS);
  radio.flush_rx();
  radio.flush_tx();
  radio.stopListening();
  drawBootProgress(94, F("RADIO CONFIGURED"));

  command.magic = COMMAND_MAGIC;
  command.version = PROTOCOL_VERSION;
  telemetry.magic = 0;

  // GPIO3/RX must not be used as an output while the NodeMCU USB-UART bridge
  // drives it. GPIO1/TX is safe after startup logging is finished; the bridge's
  // receive side is an input. End Serial before handing TX to the buzzer.
  Serial.flush();
  Serial.end();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  drawBootProgress(100, F("REMOTE READY"));
  delay(650);

  tft.fillScreen(UI_BG);
  lastCommandAt = millis() - COMMAND_PERIOD_MS;
  randomSeed(micros());
}

void loop() {
  uint32_t now = millis();

  if (appMode == APP_DOTFRAME) {
    if (now - lastCommandAt >= COMMAND_PERIOD_MS) {
      lastCommandAt = now;
      updateSelectButton(readPcf8574());
    }
    DotFrameMode::loop();
    return;
  }

  if (now - lastCommandAt >= COMMAND_PERIOD_MS) {
    lastCommandAt = now;
    // Read ADS1115 and PCF8574 only at the 40 Hz command rate. The previous
    // version hammered the software I2C bus continuously between packets.
    readControls();
    sendCommand();
  }

  if (easterRequested) {
    easterRequested = false;
    enterEasterMenu();
  }

  if (appMode != APP_REMOTE) {
    updateEasterApps();
  }

  if (appMode == APP_REMOTE && now - lastScreenAt >= SCREEN_PERIOD_MS) {
    lastScreenAt = now;
    drawScreen();
  }

  yield();
}
