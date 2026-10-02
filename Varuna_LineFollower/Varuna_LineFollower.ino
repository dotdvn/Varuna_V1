/* PROJECT VARUNA — ESP32 VEHICLE FIRMWARE */
#include <Arduino.h>
#include <esp_system.h>
#include <SPI.h>
#include <Wire.h>
#include <RF24.h>
#include <TinyGPSPlus.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Config.h"
#include "RadioProtocol.h"
#include "DigitalIR_Sensors.h"
#include "MotorDriver.h"
#include "PIDController.h"
#include "Buzzer.h"

using namespace VarunaRadio;
DigitalIR_Sensors sensors;
MotorDriver motors;
PIDController pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
Buzzer buzzer;
SPIClass nrfSpi(HSPI);
// A conservative SPI speed is much more reliable with breadboards, jumper
// wires, and common nRF24L01 clone modules than RF24's 10 MHz default.
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN, 2000000);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
Adafruit_MPU6050 mpu;

const uint8_t RADIO_ADDRESS[6] = "VRN01";
CommandPacket command = {};
TelemetryPacket telemetry = {};
uint32_t lastLoopMicros = 0, lastCommandAt = 0, lastTelemetryAt = 0;
bool radioCommandValid = false, mpuAvailable = false;
bool radioAvailable = false;
bool autonomousFallback = false;
bool radioFaultAnnounced = false;
bool sensorsCalibrated = false;
bool wirelessWhitePending = false;
bool wirelessBlackPending = false;
bool autonomousRaceStarted = false;
uint32_t lastCommandReportAt = 0;
uint32_t radioWaitStarted = 0;

static void pollRadio();

static bool pcfWrite(uint8_t value) {
  Wire.beginTransmission(PCF8574_ADDRESS);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static uint8_t pcfRead() {
  if (Wire.requestFrom((uint8_t)PCF8574_ADDRESS, (uint8_t)1) != 1)
    return 0xFF;
  return Wire.read();
}

static bool calibrationButtonPressed() {
  return (pcfRead() & (1U << PCF_CAL_BUTTON_PIN)) == 0;
}
bool wasLineLost = false, previousSensorState[SENSOR_COUNT] = {false};
const float dtSeconds = (float)CONTROL_LOOP_MICROS / 1000000.0f;

static void calibrateIRMode();

static bool validCommand(const CommandPacket &p) {
  return p.magic == COMMAND_MAGIC && p.version == PROTOCOL_VERSION &&
         (p.mode == MODE_MANUAL || p.mode == MODE_LINE_FOLLOWER) &&
         p.speedPwm <= 225 && p.drive >= -1000 && p.drive <= 1000 &&
         p.steering >= -1000 && p.steering <= 1000;
}

static void stopAllVehicleActions() {
  motors.stop();
  buzzer.silence();
  pid.reset();
  wasLineLost = false;
}

static void startAutonomousFallback() {
  if (!autonomousFallback) {
    autonomousFallback = true;
    command.mode = MODE_LINE_FOLLOWER;
    command.speedPwm = SPEED_STRAIGHT;
    command.flags = 0;
    buzzer.beepIRMode();
    Serial.println("[AUTO] Remote unavailable; switching to IR line-follow mode.");
  }
  if (!radioFaultAnnounced) {
    radioFaultAnnounced = true;
    buzzer.beepRadioMissing();
  }
}

static void serviceCalibrationWait() {
  updateGps();
  updateTelemetry();
  if (radioAvailable) pollRadio();
  buzzer.update();
}

static bool physicalCalibrationPress() {
  if (!calibrationButtonPressed()) return false;

  delay(50);
  if (!calibrationButtonPressed()) return false;
  while (calibrationButtonPressed()) {
    serviceCalibrationWait();
    delay(5);
  }
  delay(50);
  return true;
}

static void waitForCalibrationTrigger(bool whitePhase) {
  while (true) {
    serviceCalibrationWait();

    if (whitePhase && wirelessWhitePending) {
      wirelessWhitePending = false;
      return;
    }
    if (!whitePhase && wirelessBlackPending) {
      wirelessBlackPending = false;
      return;
    }
    if (physicalCalibrationPress()) return;
    delay(5);
  }
}

static void calibrateIRMode() {
  if (sensorsCalibrated) return;

  motors.stop();
  sensors.resetCalibration();
  Serial.println("[IR] Calibration required for line-follow mode.");
  Serial.println("WHITE surface: press vehicle button or remote SELECT.");
  waitForCalibrationTrigger(true);
  sensors.calibrateWhite();
  buzzer.beepWhiteDone();

  Serial.println("BLACK line: press vehicle button or remote SELECT.");
  waitForCalibrationTrigger(false);
  sensors.calibrateBlack();
  if (sensors.isCalibrated()) {
    buzzer.beepBlackDone();
    sensorsCalibrated = true;
    buzzer.beepCalibrationReady();
    Serial.println("[IR] Calibration complete; line following enabled.");
  } else {
    sensorsCalibrated = false;
    buzzer.beepLineLost();
    Serial.println("[IR] Calibration rejected; repeat white and black samples.");
  }
}

static void waitForManualRaceStart() {
  motors.stop();
  autonomousRaceStarted = false;
  Serial.println("[IR] Calibration complete; press vehicle button to start race.");
  Serial.println("[IR] Waiting for manual race start...");

  // Require the calibration button to be released before accepting the start
  // press, so the black-sample button press cannot start the race accidentally.
  while (calibrationButtonPressed()) {
    serviceCalibrationWait();
    delay(5);
  }

  while (!autonomousRaceStarted) {
    serviceCalibrationWait();
    if (physicalCalibrationPress()) {
      autonomousRaceStarted = true;
      buzzer.beepCalibrationReady();
      Serial.println("[IR] Manual race start accepted.");
    }
    delay(5);
  }
}

static void waitForRemoteOrFallback() {
  radioWaitStarted = millis();
  Serial.println("[RADIO] Waiting for remote command...");
  while (!radioCommandValid && millis() - radioWaitStarted < RADIO_CONNECT_GRACE_MS) {
    updateGps();
    updateTelemetry();
    if (radioAvailable) pollRadio();
    buzzer.update();
    delay(5);
  }

  if (!radioCommandValid) {
    startAutonomousFallback();
  }
}

static void updateGps() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
}

static void updateTelemetry() {
  uint32_t now = millis();
  if (now - lastTelemetryAt < TELEMETRY_PERIOD_MS) return;
  lastTelemetryAt = now;
  telemetry.magic = TELEMETRY_MAGIC;
  telemetry.version = PROTOCOL_VERSION;
  telemetry.sequence++;
  telemetry.flags = 0;
  telemetry.reserved = 0;

  uint32_t adcMv = analogReadMilliVolts(BATTERY_ADC_PIN);
  telemetry.batteryMv = constrain(
      (uint32_t)(adcMv * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION),
      0UL, 65535UL);
  if (telemetry.batteryMv < BATTERY_LOW_MV)
    telemetry.flags |= TELEMETRY_BATTERY_LOW;

  if (gps.location.isValid() && gps.location.age() < 2000) {
    telemetry.flags |= TELEMETRY_GPS_FIX;
    telemetry.latitudeE7 = (int32_t)llround(gps.location.lat() * 10000000.0);
    telemetry.longitudeE7 = (int32_t)llround(gps.location.lng() * 10000000.0);
    telemetry.satellites = gps.satellites.isValid()
        ? min((uint32_t)255, gps.satellites.value()) : 0;
    telemetry.gpsSpeedCms = gps.speed.isValid()
        ? min((uint32_t)65535,
              (uint32_t)lround(gps.speed.mps() * 100.0)) : 0;
  } else {
    telemetry.latitudeE7 = telemetry.longitudeE7 = 0;
    telemetry.satellites = 0;
    telemetry.gpsSpeedCms = 0;
  }

  if (mpuAvailable) {
    sensors_event_t accel, gyro, temperature;
    mpu.getEvent(&accel, &gyro, &temperature);
    constexpr float TO_MG = 1000.0f / 9.80665f;
    telemetry.accelXmg = constrain((int)lround(accel.acceleration.x * TO_MG), -32768, 32767);
    telemetry.accelYmg = constrain((int)lround(accel.acceleration.y * TO_MG), -32768, 32767);
    telemetry.accelZmg = constrain((int)lround(accel.acceleration.z * TO_MG), -32768, 32767);
    float roll = atan2f(accel.acceleration.y, accel.acceleration.z) * RAD_TO_DEG;
    float pitch = atan2f(-accel.acceleration.x,
        sqrtf(accel.acceleration.y * accel.acceleration.y +
              accel.acceleration.z * accel.acceleration.z)) * RAD_TO_DEG;
    telemetry.rollCdeg = constrain((int)lround(roll * 100.0f), -32768, 32767);
    telemetry.pitchCdeg = constrain((int)lround(pitch * 100.0f), -32768, 32767);
    telemetry.flags |= TELEMETRY_MPU_OK;
  } else {
    telemetry.accelXmg = telemetry.accelYmg = telemetry.accelZmg = 0;
    telemetry.rollCdeg = telemetry.pitchCdeg = 0;
  }
}

static void pollRadio() {
  bool received = false;
  CommandPacket incoming;
  while (radio.available()) {
    uint8_t size = radio.getDynamicPayloadSize();
    if (size == sizeof(CommandPacket)) {
      radio.read(&incoming, sizeof(incoming));
      received = true;
    } else if (size > 0 && size <= 32) {
      uint8_t discard[32];
      radio.read(discard, size);
    } else radio.flush_rx();
  }
  if (received && validCommand(incoming)) {
    command = incoming;
    lastCommandAt = millis();
    radioCommandValid = true;
    if (millis() - lastCommandReportAt >= 1000) {
      lastCommandReportAt = millis();
      Serial.printf("[RADIO] Command mode=%u speed=%u drive=%d steer=%d\n",
                    command.mode, command.speedPwm, command.drive,
                    command.steering);
    }
    if (incoming.flags & COMMAND_CALIBRATE_WHITE) {
      wirelessWhitePending = true;
      wirelessBlackPending = false;
      Serial.println("[CAL] Remote WHITE trigger accepted.");
    } else if (incoming.flags & COMMAND_CALIBRATE_BLACK) {
      wirelessBlackPending = true;
      wirelessWhitePending = false;
      Serial.println("[CAL] Remote BLACK trigger accepted.");
    }
    if (autonomousFallback) {
      autonomousFallback = false;
      radioFaultAnnounced = false;
      Serial.println("[RADIO] Remote command received; returning to remote mode.");
    }
  }
  if (received) {
    radio.flush_tx();
    radio.writeAckPayload(1, &telemetry, sizeof(telemetry));
  }
}

static void driveManual() {
  int maxPwm = command.speedPwm;
  int throttle = (int32_t)command.drive * maxPwm / 1000;
  int turn = (int32_t)command.steering * maxPwm / 1000;
  int leftMotor;
  int rightMotor;
  if (throttle == 0 && turn != 0) {
    // Joystick centered with steering applied: pivot in place like a tank.
    leftMotor = turn;
    rightMotor = -turn;
  } else {
    // While moving, keep both sides active and mix throttle with steering.
    leftMotor = throttle + turn;
    rightMotor = throttle - turn;
  }
  leftMotor = constrain(leftMotor, -maxPwm, maxPwm);
  rightMotor = constrain(rightMotor, -maxPwm, maxPwm);
  motors.drive(leftMotor, rightMotor);
}

static void driveLineFollower() {
  sensors.update();
  for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
    bool active = sensors.getRaw(i) == 1;
    if (active && !previousSensorState[i]) buzzer.sensorTriggered(i);
    previousSensorState[i] = active;
  }

  int left = 0, right = 0;
  if (sensors.isLineLost()) {
    if (!wasLineLost) { buzzer.beepLineLost(); wasLineLost = true; }
    if (sensors.getLastLineSide() == SIDE_LEFT) {
      left = LINE_LOSS_INNER_PWM; right = LINE_LOSS_OUTER_PWM;
    } else {
      left = LINE_LOSS_OUTER_PWM; right = LINE_LOSS_INNER_PWM;
    }
    pid.reset();
  } else {
    wasLineLost = false;
    int error = sensors.getPosition(), absError = abs(error);
    if (absError > ERROR_SHARP_THRESH) {
      if (error < 0) { left = SHARP_TURN_INNER_PWM; right = SHARP_TURN_OUTER_PWM; }
      else { left = SHARP_TURN_OUTER_PWM; right = SHARP_TURN_INNER_PWM; }
    } else {
      int base = SPEED_SHARP_TURN;
      if (absError < ERROR_STRAIGHT_THRESH) base = SPEED_STRAIGHT;
      else if (absError < ERROR_SWEEP_THRESH) base = SPEED_SWEEP_TURN;
      else if (absError < ERROR_SMALL_THRESH) base = SPEED_SMALL_TURN;
      else if (absError < ERROR_MED_THRESH) base = SPEED_MED_TURN;
      base = min(base, (int)command.speedPwm);
      int correction = pid.compute(error, dtSeconds);
      left = base + correction; right = base - correction;
    }
  }
  int limit = command.speedPwm;
  motors.drive(constrain(left, -limit, limit), constrain(right, -limit, limit));
}

void setup() {
  Serial.begin(115200);
  esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("[BOOT] ESP32 reset reason: %d\n", (int)resetReason);
  pinMode(NRF_CSN_PIN, OUTPUT); digitalWrite(NRF_CSN_PIN, HIGH);
  pinMode(NRF_CE_PIN, OUTPUT); digitalWrite(NRF_CE_PIN, LOW);
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  buzzer.begin(); motors.begin(); motors.stop(); sensors.begin(); pid.reset();
  buzzer.bootChime();

  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  if (pcfWrite(0xFF))
    Serial.println("[OK] Vehicle PCF8574 detected at 0x27");
  else
    Serial.println("[ERROR] Vehicle PCF8574 not found at 0x27");
  mpuAvailable = mpu.begin(0x68, &Wire);
  if (mpuAvailable) {
    Serial.println("[OK] MPU6050 detected at 0x68");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else Serial.println("[ERROR] MPU6050 not detected at 0x68; horizon telemetry disabled.");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, -1);

  nrfSpi.begin(NRF_SCK_PIN, NRF_MISO_PIN, NRF_MOSI_PIN, NRF_CSN_PIN);
  // Some nRF24 boards need extra settling time after the ESP32's 3.3 V rail
  // rises. Retry initialization before declaring the module missing.
  delay(150);
  for (uint8_t attempt = 1; attempt <= 3 && !radioAvailable; ++attempt) {
    radioAvailable = radio.begin(&nrfSpi);
    if (!radioAvailable) {
      Serial.print("[WARN] nRF24 init attempt ");
      Serial.print(attempt);
      Serial.println(" failed");
      digitalWrite(NRF_CE_PIN, LOW);
      digitalWrite(NRF_CSN_PIN, HIGH);
      delay(200);
    }
  }
  if (radioAvailable) {
    if (!radio.isChipConnected()) {
      radioAvailable = false;
      Serial.println("[ERROR] nRF24 initialized but chip is not connected.");
    }
  }
  if (radioAvailable) {
    radio.setAutoAck(true); radio.enableAckPayload(); radio.enableDynamicPayloads();
    radio.setAddressWidth(5);
    radio.setRetries(5, 15);
    radio.setCRCLength(RF24_CRC_16);
    // Channel 76 = 2476 MHz, inside the 2.4 GHz ISM band. 250 kbps gives
    // substantially better sensitivity and reliability for vehicle control.
    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_250KBPS);
    radio.setChannel(76);
    radio.openReadingPipe(1, RADIO_ADDRESS);
    radio.flush_rx();
    radio.flush_tx();
    radio.powerUp();
    delay(5);
    updateTelemetry();
    radio.writeAckPayload(1, &telemetry, sizeof(telemetry));
    radio.startListening();
    Serial.println("[OK] nRF24 ready, chip connected: YES");
  } else {
    Serial.println("[ERROR] nRF24 missing; IR fallback will start after calibration.");
  }

  stopAllVehicleActions();
  waitForRemoteOrFallback();
  if (autonomousFallback || (radioCommandValid && command.mode == MODE_LINE_FOLLOWER)) {
    calibrateIRMode();
    if (autonomousFallback && sensorsCalibrated) waitForManualRaceStart();
  }
  lastLoopMicros = micros();
  Serial.println(autonomousFallback ? "Ready; IR calibrated, waiting for race start."
                                    : "Ready; remote control active.");
}

void loop() {
  updateGps(); updateTelemetry();
  if (radioAvailable) pollRadio();
  buzzer.update();
  uint32_t now = micros();
  if (now - lastLoopMicros < CONTROL_LOOP_MICROS) return;
  lastLoopMicros = now;

  uint32_t nowMillis = millis();
  if (wirelessWhitePending || wirelessBlackPending) {
    command.mode = MODE_LINE_FOLLOWER;
    command.speedPwm = SPEED_STRAIGHT;
    command.flags = 0;
    sensorsCalibrated = false;
    calibrateIRMode();
    return;
  }

  bool timedOut = !radioCommandValid || nowMillis - lastCommandAt > RADIO_TIMEOUT_MS;
  if (!autonomousFallback &&
      (!radioAvailable || (timedOut && nowMillis - radioWaitStarted >= RADIO_CONNECT_GRACE_MS))) {
    startAutonomousFallback();
  }

  if (autonomousFallback) {
    command.mode = MODE_LINE_FOLLOWER;
    command.speedPwm = SPEED_STRAIGHT;
    command.flags = 0;
    if (!sensorsCalibrated) {
      calibrateIRMode();
      if (!sensorsCalibrated) return;
    }
    if (!autonomousRaceStarted) {
      waitForManualRaceStart();
      if (!autonomousRaceStarted) return;
    }
    driveLineFollower();
    return;
  }

  if (timedOut || (command.flags & COMMAND_ESTOP) || command.speedPwm == 0) {
    stopAllVehicleActions();
    return;
  }
  if (command.mode == MODE_MANUAL) {
    pid.reset(); wasLineLost = false; driveManual();
  } else {
    calibrateIRMode();
    driveLineFollower();
  }
}
