#ifndef VARUNA_RADIO_PROTOCOL_H
#define VARUNA_RADIO_PROTOCOL_H

#include <Arduino.h>

namespace VarunaRadio {

constexpr uint16_t COMMAND_MAGIC = 0x5643;
constexpr uint16_t TELEMETRY_MAGIC = 0x5654;
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
  uint8_t speedPwm;
  uint8_t flags;
  int16_t drive;
  int16_t steering;
};

struct TelemetryPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t flags;
  uint16_t sequence;
  uint16_t batteryMv;
  int32_t latitudeE7;
  int32_t longitudeE7;
  uint8_t satellites;
  uint8_t reserved;
  int16_t accelXmg;
  int16_t accelYmg;
  int16_t accelZmg;
  int16_t rollCdeg;
  int16_t pitchCdeg;
  uint16_t gpsSpeedCms;
};
#pragma pack(pop)

static_assert(sizeof(CommandPacket) <= 32, "Command packet exceeds nRF24 limit");
static_assert(sizeof(TelemetryPacket) <= 32, "Telemetry packet exceeds nRF24 limit");

} // namespace VarunaRadio

#endif
