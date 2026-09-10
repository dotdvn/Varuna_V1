# Varuna V1 Pin Configuration

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
| Calibration button | GPIO32 | Button to GND, `INPUT_PULLUP` |
| Buzzer | GPIO4 | Buzzer signal, common GND |
| Left BTS7960 RPWM | GPIO18 | Forward PWM |
| Left BTS7960 LPWM | GPIO19 | Reverse PWM |
| Right BTS7960 RPWM | GPIO16 | Forward PWM |
| Right BTS7960 LPWM | GPIO17 | Reverse PWM |

## Boot-Safe Notes

- Sensor inputs use GPIO13, GPIO14, GPIO25, GPIO26, GPIO27, GPIO33, GPIO34, and GPIO35.
- GPIO34 and GPIO35 are input-only and are suitable for digital sensor `DO` outputs.
- GPIO0, GPIO2, GPIO5, GPIO12, and GPIO15 are not used for sensors because they can affect ESP32 boot.
- GPIO6-GPIO11 are reserved for ESP32 flash and must not be used.
- Power the sensor modules from the correct voltage and share a common ground with the ESP32.

## Sensor Logic

The firmware calibrates white and black surfaces at startup and automatically detects each sensor's polarity.
During calibration, place all sensors over white, press the button, then place them over the black line and press it again.
