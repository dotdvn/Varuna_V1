# Varuna V1 Current Pin Configuration

This document matches `Varuna_LineFollower/Config.h`. GPIO numbers are ESP32
GPIO numbers.

## Digital IR Sensors

The eight digital IR sensor modules are ordered physically from **LEFT to RIGHT**:

| Position | Sensor | ESP32 GPIO | Weight |
|---|---:|---:|---:|
| Far left | S1 | GPIO13 | -3500 |
| Left outer | S2 | GPIO14 | -2500 |
| Left inner | S3 | GPIO25 | -1500 |
| Left center | S4 | GPIO26 | -500 |
| Right center | S5 | GPIO27 | +500 |
| Right inner | S6 | GPIO33 | +1500 |
| Right outer | S7 | GPIO34 | +2500 |
| Far right | S8 | GPIO35 | +3500 |

### Physical Layout

```text
LEFT                                                        RIGHT
 S1        S2        S3        S4        S5        S6        S7        S8
GPIO13    GPIO14    GPIO25    GPIO26    GPIO27    GPIO33    GPIO34    GPIO35
-3500     -2500     -1500     -500      +500     +1500     +2500     +3500
```

Connect each IR module's `DO` output to its GPIO. Connect every module's `GND` to ESP32 `GND`.
Do not connect a 5 V digital output directly to an ESP32 GPIO; use a level shifter or voltage divider when required.

## Other Connections

| Function | ESP32 GPIO | Connection |
|---|---:|---|
| Calibration button | PCF8574 P0 | Button to GND, active LOW |
| Buzzer | GPIO4 | Buzzer signal, common GND |
| Left BTS7960 RPWM | GPIO18 | Reverse PWM |
| Left BTS7960 LPWM | GPIO19 | Forward PWM |
| Right BTS7960 RPWM | GPIO16 | Reverse PWM |
| Right BTS7960 LPWM | GPIO17 | Forward PWM |

## nRF24L01+ Radio

| Radio pin | ESP32 GPIO | Notes |
|---|---:|---|
| CE | GPIO23 | Radio enable |
| CSN | GPIO32 | SPI chip select |
| SCK | GPIO5 | HSPI clock |
| MISO | GPIO12 | HSPI data from radio |
| MOSI | GPIO15 | HSPI data to radio |
| IRQ | Not connected | Optional |
| VCC | 3.3 V | Never 5 V |
| GND | GND | Common ground |

The vehicle radio uses address `VRN01`, channel 76, 250 kbps, and 2 MHz SPI.
Use a 10-100 uF capacitor directly across the nRF24L01+ VCC and GND pins.

## Vehicle I2C

| Device | ESP32 connection |
|---|---|
| Vehicle PCF8574 address `0x27` | SDA GPIO21, SCL GPIO22 |
| Calibration button | PCF8574 P0 to GND |
| MPU6050 address `0x68` | SDA GPIO21, SCL GPIO22 |

## GPS and Battery Monitor

| Device / signal | ESP32 connection |
|---|---|
| NEO-6M TX | GPIO39 (ESP32 RX), 9600 baud |
| NEO-6M RX | Not connected |
| Battery sensor signal | GPIO36, maximum 3.3 V |

## Boot-Safe Notes

- Sensor inputs use GPIO13, GPIO14, GPIO25, GPIO26, GPIO27, GPIO33, GPIO34, and GPIO35.
- GPIO32 is reserved for nRF24 CSN; it is not the calibration-button input.
- GPIO23 is reserved for nRF24 CE.
- GPIO34 and GPIO35 are input-only and are suitable for digital sensor `DO` outputs.
- GPIO0, GPIO2, GPIO5, GPIO12, and GPIO15 are not used for sensors because they can affect ESP32 boot.
- GPIO6-GPIO11 are reserved for ESP32 flash and must not be used.
- Power the sensor modules from the correct voltage and share a common ground with the ESP32.

## Sensor Logic

The firmware calibrates white and black surfaces at startup and automatically detects each sensor's polarity.
During calibration, place all sensors over white, press the button, then place them over the black line and press it again.
