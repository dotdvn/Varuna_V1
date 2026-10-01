#ifndef VARUNA_RADIO_PROTOCOL_H
#define VARUNA_RADIO_PROTOCOL_H

#include <Arduino.h>

// Both the ESP8266 remote and ESP32 vehicle must use this exact file.
// nRF24L01 packets have a maximum payload size of 32 bytes.
namespace VarunaRadio {

constexpr uint16_t COMMAND_MAGIC = 0x5643;   // "VC"
constexpr uint16_t TELEMETRY_MAGIC = 0x5654; // "VT"
constexpr uint8_t PROTOCOL_VERSION = 1;

enum DriveMode : uint8_t {
  MODE_MANUAL = 0,
  MODE_LINE_FOLLOWER = 1
};

enum CommandFlags : uint8_t {
  COMMAND_ESTOP = 1 << 0,
  COMMAND_CALIBRATE_WHITE = 1 << 1,
  COMMAND_CALIBRATE_BLACK = 1 << 2
};

enum TelemetryFlags : uint8_t {
  TELEMETRY_GPS_FIX = 1 << 0,
  TELEMETRY_MPU_OK = 1 << 1,
  TELEMETRY_BATTERY_LOW = 1 << 2
};

#pragma pack(push, 1)
struct CommandPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t mode;
  uint16_t sequence;
  uint8_t speedPwm;     // Slider-selected maximum: 60..225
    uint8_t speedPwm;     // Slider-selected maximum: 60..225
  uint8_t flags;
  int16_t drive;        // -1000 reverse, 0 neutral, +1000 forward
  int16_t steering;     // -1000 left, 0 center, +1000 right
};

struct TelemetryPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t flags;
  uint16_t sequence;
  uint16_t batteryMv;
  int32_t latitudeE7;   // Decimal degrees multiplied by 10,000,000
  int32_t longitudeE7;
  uint8_t satellites;
  uint8_t reserved;
  int16_t accelXmg;     // MPU6050 acceleration in milli-g
  int16_t accelYmg;
  int16_t accelZmg;
  int16_t rollCdeg;     // Roll in hundredths of a degree
  int16_t pitchCdeg;    // Pitch in hundredths of a degree
  uint16_t gpsSpeedCms; // GPS ground speed in centimetres/second
};
#pragma pack(pop)

static_assert(sizeof(CommandPacket) <= 32, "Command packet exceeds nRF24 limit");
static_assert(sizeof(TelemetryPacket) <= 32, "Telemetry packet exceeds nRF24 limit");

} // namespace VarunaRadio

#endif
