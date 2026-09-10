# Varuna V1 Pin Configuration

## Digital IR Sensors

The seven digital IR sensor modules are ordered physically from **LEFT to RIGHT**:

| Position | Sensor | ESP32 GPIO | Weight |
|---|---:|---:|---:|
| Far left | S1 | GPIO13 | -3000 |
| Left inner | S2 | GPIO14 | -2000 |
| Left center | S3 | GPIO25 | -1000 |
| Center | S4 | GPIO26 | 0 |
| Right center | S5 | GPIO27 | +1000 |
| Right inner | S6 | GPIO33 | +2000 |
| Far right | S7 | GPIO34 | +3000 |

### Physical Layout

```text
LEFT                                                        RIGHT
 S1        S2        S3        S4        S5        S6        S7
GPIO13    GPIO14    GPIO25    GPIO26    GPIO27    GPIO33    GPIO34
-3000     -2000     -1000        0     +1000     +2000     +3000
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

- Sensor inputs use GPIO13, GPIO14, GPIO25, GPIO26, GPIO27, GPIO33, and GPIO34.
- GPIO34 is input-only and is suitable for a digital sensor `DO` output.
- GPIO0, GPIO2, GPIO5, GPIO12, and GPIO15 are not used for sensors because they can affect ESP32 boot.
- GPIO6-GPIO11 are reserved for ESP32 flash and must not be used.
- Power the sensor modules from the correct voltage and share a common ground with the ESP32.

## Sensor Logic

The firmware calibrates white and black surfaces at startup and automatically detects each sensor's polarity.
During calibration, place all sensors over white, press the button, then place them over the black line and press it again.
