# Varuna ESP8266 Remote Firmware

## Assumed hardware

- NodeMCU ESP8266
- ST7735 1.8-inch 128x160 TFT
- nRF24L01+
- ADS1115 at I2C address `0x48`
- PCF8574 at I2C address `0x27`
- Two-axis analog joystick
- Analog slider
- Momentary mode button

## Wiring

| Device | Signal | NodeMCU / ADS1115 |
|---|---|---|
| TFT | SCK | D5 / GPIO14 |
| TFT | MOSI | D7 / GPIO13 |
| TFT | CS | D2 / GPIO4 |
| TFT | DC | D1 / GPIO5 |
| TFT | RST | NodeMCU RST |
| TFT | VCC and BL | 3.3 V |
| nRF24L01 | CE | D8 / GPIO15 |
| nRF24L01 | CSN | D0 / GPIO16 |
| nRF24L01 | SCK | D5 / GPIO14 |
| nRF24L01 | MOSI | D7 / GPIO13 |
| nRF24L01 | MISO | D6 / GPIO12 |
| ADS1115 | SCL | D3 / GPIO0 |
| ADS1115 | SDA | D4 / GPIO2 |
| Slider | Wiper | ADS1115 A0 |
| PCF8574 | SCL | D3 / GPIO0 |
| PCF8574 | SDA | D4 / GPIO2 |
| Mode button | Signal | PCF8574 P0, active LOW |
| Select button | Signal | PCF8574 P1, active LOW |
| Buzzer | Signal | TX / GPIO1 |
| Joystick | VRX | ADS1115 A2 (physical forward/reverse axis) |
| Joystick | VRY | ADS1115 A3 (physical left/right axis) |

Connect each PCF8574 button between its assigned P pin and GND. ADS1115 A1 is
unused. Connect all grounds together. Place a 10-100 uF capacitor directly
across the nRF24L01 VCC and GND pins.

Because the remote uses ESP8266 GPIO15 for nRF24 CE, keep CE LOW during reset
with a hardware pulldown (about 10 kOhm). Do not allow the radio or another
device to drive CE HIGH during boot.

## Required Arduino libraries

- RF24 by TMRh20
- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- Adafruit ADS1X15
- WebSockets by Markus Sattler

Select **NodeMCU 1.0 (ESP-12E Module)** in the Arduino IDE.
The firmware uses raw GPIO numbers internally, so it can also compile with the
**Generic ESP8266 Module** profile; the physical D-pin wiring remains unchanged.

At power-on, keep the joystick centered while the remote measures its neutral
position. The slider selects PWM 60-225. Each mode-button press changes between
position. The slider selects PWM 60-225. Each mode-button press changes between
manual control and autonomous line-following.

The landscape dashboard shows a large speed value, a live joystick target,
radio-link bars, battery gauge, GPS fix/satellite count, and a red/blue
artificial-horizon sphere driven by MPU6050 roll and pitch.

Boot uses two uncluttered slides. Slide one identifies Project Varuna, Dotbyte x
PVN Company, and the developers Devan, Arush, Parameswar, and Vinay. Slide two
is reserved for the progress bar, which advances from actual initialization
events: display startup, I2C startup, ADS1115 detection, PCF8574 detection,
joystick calibration, nRF24 detection, and radio configuration. The PCF8574
address is fixed at `0x27`.

## Varuna Easter Eggs

Hold the PCF8574 P1 Select button for five seconds to open **Varuna Easter
Eggs**. Use joystick up/down to choose an item and tap Select to open it. Hold
Select for five seconds to leave the menu or a game.

When the vehicle is in line-follow mode, a short Select press triggers wireless
calibration. The first short press samples white; the second samples black.
The vehicle calibration button on PCF8574 P0 can be used for either phase too,
so calibration can be completed with a hybrid of remote and physical presses.

- **Star Catch** — steer the catcher left/right, collect falling stars, and tap
  Select to pause/resume.
- **Reaction Rush** — press Select when the signal changes to green.
- **Neon Snake** — steer in four directions, collect orange food, and avoid the
  walls and your own trail.
- **Orbit Dodge** — move between three lanes and evade incoming obstacles.
- **DotFrame** — full browser-to-TFT video streaming.

The remote continuously transmits a redundant safe command while the Easter
Egg menu or a game is open: manual mode, zero PWM, zero drive, zero steering,
and the emergency-stop flag. Before DotFrame enables Wi-Fi, it sends these stop
packets for one second and powers down the nRF24 radio. DotFrame creates the
private Wi-Fi network `Varuna-DotFrame` with password `varuna01`; connect a
phone or computer and open `http://192.168.4.1`. Hold Select for five seconds to
exit DotFrame; the remote restarts so NRF control returns in a known-safe state.

Easter Egg screens use dirty-region rendering: menu navigation redraws only the
two changed rows, and Star Catch erases/redraws only its moving sprites. The
DotFrame streamer never clears the TFT between video strips, preventing the
full-screen flashing caused by repeated background redraws.

The vehicle should listen on address `VRN01`, validate `CommandPacket`, and stop
the motors if valid commands stop arriving. It should preload its latest
`TelemetryPacket` with `writeAckPayload()` before acknowledging the next command.
