# Varuna V1 ESP32 Vehicle Pinout

This is the current pinout for the firmware in `Varuna_LineFollower/Config.h`.
All GPIO numbers below are ESP32 GPIO numbers, not board header labels.

## Existing line-follower hardware

| Function | ESP32 GPIO |
|---|---:|
| IR S1, far left | 13 |
| IR S2 | 14 |
| IR S3 | 25 |
| IR S4 | 26 |
| IR S5 | 27 |
| IR S6 | 33 |
| IR S7 | 34 |
| IR S8, far right | 35 |
| Calibration button to GND | PCF8574 P0 |
| Buzzer | 4 |
| Left BTS7960 RPWM | 18 |
| Left BTS7960 LPWM | 19 |
| Right BTS7960 RPWM | 16 |
| Right BTS7960 LPWM | 17 |

## nRF24L01+ Radio

| nRF24L01+ pin | ESP32 GPIO |
|---|---:|
| CE | 23 |
| CSN | 32 |
| SCK | 5 |
| MISO | 12 |
| MOSI | 15 |
| IRQ | Not connected |
| VCC | 3.3 V only |
| GND | GND |

The radio uses SPI HSPI at 2 MHz, channel 76, and 250 kbps. Both radio modules
must use the same settings and address `VRN01`. Place a 10-100 uF capacitor
directly across the radio VCC and GND pins.

## Vehicle PCF8574

| PCF8574 pin | ESP32 connection |
|---|---:|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3 V |
| GND | GND |
| P0 | Calibration button to GND |

Set the PCF8574 address jumpers for `0x27`. It shares GPIO21/GPIO22 with the
MPU6050. Both boards must run at 3.3 V logic.

## MPU6050

| MPU6050 pin | ESP32 GPIO |
|---|---:|
| SDA | 21 |
| SCL | 22 |
| VCC | 3.3 V |
| GND | GND |
| AD0 | GND for address `0x68` |
| INT | Not connected |

## NEO-6M GPS

| NEO-6M pin | ESP32 connection |
|---|---|
| TX | GPIO39 (ESP32 RX) |
| RX | Not connected |
| GND | GND |
| VCC | According to the GPS breakout specification |

The firmware receives GPS data at 9600 baud. GPIO39 is input-only, which is
appropriate for the GPS TX signal. Confirm that the GPS TX logic level does not
exceed 3.3 V.

## Battery-voltage sensor

| Sensor pin | ESP32 connection |
|---|---|
| Signal / S | GPIO36 |
| GND / - | GND |
| Positive input / + | Battery positive |

The default firmware assumes a common 0-25 V sensor module with a 5:1 divider.
Measure the actual battery with a multimeter and adjust
`BATTERY_DIVIDER_RATIO` and `BATTERY_CALIBRATION` in `Config.h`. The voltage at
GPIO36 must never exceed 3.3 V.

## Power and Ground

- nRF24L01+, MPU6050, PCF8574, and ESP32 logic use 3.3 V logic.
- The motor battery must not be connected directly to an ESP32 GPIO or 3.3 V pin.
- Connect ESP32, sensors, radio, motor-driver logic ground, and battery negative
  to a common ground reference.

## Safety behaviour

- Motors stay stopped until calibration is complete. If the remote is missing,
  the vehicle gives a descending warning chime and enters IR line-follow mode.
- If valid remote packets stop arriving, the vehicle enters IR fallback after
  the configured grace period.
- `COMMAND_ESTOP`, zero PWM, or an invalid command stops all vehicle actions.
- Manual mode uses arcade mixing from joystick drive and steering.
- Line-follow mode uses the existing sensor/PID algorithm with the remote's
  selected PWM as the motor-output limit.
- Remote SELECT short presses can trigger white and black calibration; the
  physical PCF8574 P0 button can trigger either phase as well.
- GPS, battery, acceleration, roll, and pitch are returned to the remote as
  nRF24 acknowledgement telemetry.
