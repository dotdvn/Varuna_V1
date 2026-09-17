# Varuna V1 Hardware Handoff

## System

- ESP32 38-pin Classic WROOM
- Eight direct digital IR sensors
- Two BTS7960 motor drivers
- Four 400 RPM DC motors
- 12 V motor battery with a separate regulated logic supply
- Common ground between ESP32, sensors, motor drivers, and battery negative

## Sensor Pinout

Sensors are physically ordered from **LEFT to RIGHT**:

| Sensor | ESP32 GPIO | Weight |
|---|---:|---:|
| S1 far left | GPIO13 | -3500 |
| S2 left outer | GPIO14 | -2500 |
| S3 left inner | GPIO25 | -1500 |
| S4 left center | GPIO26 | -500 |
| S5 right center | GPIO27 | +500 |
| S6 right inner | GPIO33 | +1500 |
| S7 right outer | GPIO34 | +2500 |
| S8 far right | GPIO35 | +3500 |

Connect each IR module `DO` output to its listed GPIO. Connect all sensor grounds to ESP32 GND. Use a level shifter or voltage divider if a sensor `DO` output is 5 V.

GPIO34 and GPIO35 are input-only and are suitable for digital sensor outputs. The firmware reads every sensor directly from the ESP32.

## Other Pinout

| Function | GPIO |
|---|---:|
| Calibration button, active LOW | 32 |
| Buzzer | 4 |
| Left BTS7960 RPWM | 18 |
| Left BTS7960 LPWM | 19 |
| Right BTS7960 RPWM | 16 |
| Right BTS7960 LPWM | 17 |

The calibration button connects between GPIO32 and GND. The firmware enables the internal pull-up.

## Boot and Power Safety

- Do not use GPIO0, GPIO2, GPIO5, GPIO12, or GPIO15 for sensors because they can affect ESP32 boot.
- GPIO6 through GPIO11 are reserved for ESP32 flash.
- Never connect the 12 V motor battery to ESP32 GPIO, sensor logic, or the ESP32 3.3 V rail.
- Power motors through the BTS7960 motor supply and use a regulated supply for logic and sensors.
- Tie all grounds together.

## Calibration

1. Power on the robot with motors stopped.
2. Place every sensor over a white surface and press the GPIO32 button.
3. Place the sensor array over the black line and press the button again.
4. The firmware detects each sensor's output polarity and begins line following.

## Project Files

- `Varuna_LineFollower/Varuna_LineFollower.ino` - main controller
- `Varuna_LineFollower/DigitalIR_Sensors.h/.cpp` - direct sensor reading and calibration
- `Varuna_LineFollower/MotorDriver.h/.cpp` - BTS7960 motor control
- `Varuna_LineFollower/PIDController.h/.cpp` - steering controller
- `Sensor_Test/Sensor_Test.ino` - direct GPIO sensor diagnostic
- `Motor_Test/Motor_Test.ino` - motor diagnostic
- `PIN_CONFIGURATION.md` - labeled pin reference
- `PIN_CONFIGURATION.odt` - OpenDocument pin reference
