# PROJECT VARUNA 1.0 — HARDWARE & CODEBASE HANDOFF DOCUMENT
**Competition**: Mini Electric Vehicle Competition 3.0  
**Main Event**: Autonomous Line Follower Race  
**Target Platform**: ESP32 (38-Pin Classic WROOM Board)  

---

## 1. Executive Summary & Hardware Specifications

Project Varuna has been adapted specifically for high-speed autonomous line-following competition. The platform utilizes dual BTS7960 high-power motor drivers operating at 20 kHz PWM frequency, controlling 4 × 400 RPM DC motors with 100 mm diameter wheels. Sensor input is managed via a 16-channel array (2 × 8-channel QTR-8 modules) multiplexed over I2C using two PCF8574 I/O expanders to preserve ESP32 GPIOs and eliminate pin contention.

| Component | Specification / Part | Quantity | Details / Configuration |
| :--- | :--- | :--- | :--- |
| **Microcontroller** | ESP32 38-Pin WROOM Board | 1 | Classic Dual-Core 240 MHz |
| **Motors** | 400 RPM DC Motors | 4 | 2 parallel per side (Left pair / Right pair) |
| **Wheels** | 100 mm Diameter Wheels | 4 | High-traction rubber tread |
| **Motor Battery** | 12V High-Current Battery | 1 | Powers motors via BTS7960 drivers |
| **Motor Drivers** | BTS7960 High-Power H-Bridge | 2 | Left Driver (#1) & Right Driver (#2) |
| **Line Sensors** | QTR-8 Sensor Modules | 2 | 16 Channels Total (Module 1 Left, Module 2 Right) |
| **I/O Expanders** | PCF8574 I2C Modules | 2 | PCF #1 (0x20 Left), PCF #2 (0x24 Right) |

---

## 2. Complete Wiring & Pinout Reference

### A. I2C Bus & PCF8574 I/O Expanders
Both PCF8574 modules share the same I2C bus on the ESP32.

```
ESP32 GPIO21 (SDA) ──────► PCF #1 SDA  ──────► PCF #2 SDA
ESP32 GPIO22 (SCL) ──────► PCF #1 SCL  ──────► PCF #2 SCL
ESP32 3.3V / 5V    ──────► PCF #1 VCC  ──────► PCF #2 VCC
ESP32 GND          ──────► PCF #1 GND  ──────► PCF #2 GND
```

### B. PCF8574 to QTR-8 Sensor Connections

#### PCF #1 (LEFT Side — Module 1) — Intended I2C Address: `0x20`
| PCF #1 Pin | QTR-8 Module 1 Sensor | Physical Position | Weight |
| :--- | :--- | :--- | :--- |
| **P7** | M1-D8 | Far Left Outer | **-7500** |
| **P6** | M1-D7 | Left Outer | **-6500** |
| **P5** | M1-D6 | Left Mid-Outer | **-5500** |
| **P4** | M1-D5 | Left Mid | **-4500** |
| **P3** | M1-D4 | Left Mid-Inner | **-3500** |
| **P2** | M1-D3 | Left Inner | **-2500** |
| **P1** | M1-D2 | Left Center-Inner | **-1500** |
| **P0** | M1-D1 | **Left Center (Closest)** | **-500** |

#### PCF #2 (RIGHT Side — Module 2) — Intended I2C Address: `0x21`
| PCF #2 Pin | QTR-8 Module 2 Sensor | Physical Position | Weight |
| :--- | :--- | :--- | :--- |
| **P0** | M2-D8 | **Right Center (Closest)** | **+500** |
| **P1** | M2-D7 | Right Center-Inner | **+1500** |
| **P2** | M2-D6 | Right Inner | **+2500** |
| **P3** | M2-D5 | Right Mid-Inner | **+3500** |
| **P4** | M2-D4 | Right Mid | **+4500** |
| **P5** | M2-D3 | Right Mid-Outer | **+5500** |
| **P6** | M2-D2 | Right Outer | **+6500** |
| **P7** | M2-D1 | Far Right Outer | **+7500** |

> **Center Line Note**: The true center of the vehicle lies directly between **M1-D1** (-500) and **M2-D8** (+500).

---

### C. BTS7960 Motor Driver Pinouts

#### LEFT BTS7960 Driver (#1) — Controls Left 2 Parallel Motors
- **GPIO18** $\rightarrow$ `RPWM` (Forward PWM)
- **GPIO19** $\rightarrow$ `LPWM` (Reverse PWM)
- `R_EN` & `L_EN` $\rightarrow$ Tie to **5V / VCC** (Enables driver outputs)
- `VCC` $\rightarrow$ ESP32 5V / Logic VCC
- `GND` $\rightarrow$ Common System GND
- `B+` / `B-` $\rightarrow$ 12V Battery Positive / Negative
- `M+` / `M-` $\rightarrow$ Left Parallel Motor Pair

#### RIGHT BTS7960 Driver (#2) — Controls Right 2 Parallel Motors
- **GPIO16** $\rightarrow$ `RPWM` (Forward PWM)
- **GPIO17** $\rightarrow$ `LPWM` (Reverse PWM)
- `R_EN` & `L_EN` $\rightarrow$ Tie to **5V / VCC** (Enables driver outputs)
- `VCC` $\rightarrow$ ESP32 5V / Logic VCC
- `GND` $\rightarrow$ Common System GND
- `B+` / `B-` $\rightarrow$ 12V Battery Positive / Negative
- `M+` / `M-` $\rightarrow$ Right Parallel Motor Pair

---

### D. Power Distribution & Safety

> [!CAUTION]
> **NEVER connect the 12V Motor Battery directly to the ESP32 GPIOs, 3.3V line, or sensor VCC!**

1. **12V Motor Power**: Connect directly to BTS7960 `B+` and `B-` terminals.
2. **Logic Power**: Use a dedicated 5V Buck Regulator (or high-quality USB power bank/module) to supply:
   - ESP32 5V (VIN) pin
   - PCF8574 VCC (3.3V or 5V as per module spec)
   - QTR-8 Sensor VCC
   - BTS7960 Logic VCC and Enable pins (`R_EN`/`L_EN`)
3. **Common Ground**: Ensure a solid, low-resistance **GND connection** joining:
   - 12V Battery negative (-)
   - ESP32 GND
   - BTS7960 GND
   - PCF8574 GND
   - QTR-8 GND

---

## 3. PCF8574 Address Jumper Decoding & Hardware Setup

PCF8574 boards feature address-selection pads or DIP switches corresponding to hardware address bits $A_0, A_1, A_2$.

### Identifying Jumper Markings (`A2`, `J1`, `J5`, `J3`, `J4`/`J2`)
On standard blue/black PCF8574 breakout boards:
- **`J1` / `A0`**: Address bit $A_0$
- **`J2` or `J4` / `A1`**: Address bit $A_1$
- **`J3` or `J5` / `A2`**: Address bit $A_2$

#### Address Logic Table (Standard PCF8574 Chip Variant)
Base I2C Address = `0x20` ($0010 0000_2$)

| Module Target | Target Address | $A_2$ (J3/J5) | $A_1$ (J2/J4) | $A_0$ (J1) | Jumper / Solder State |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PCF #1 (LEFT)** | **`0x20`** | HIGH (1) | HIGH (1) | HIGH (1) | **All Jumpers OPEN / Default** |
| **PCF #2 (RIGHT)** | **`0x21`** | HIGH (1) | HIGH (1) | LOW (0) | **$A_0$ (J1) Shorted to GND** |

#### Address Logic Table (PCF8574A Variant Chip)
If your board features a chip marked **PCF8574A**, the base address is **`0x38`**:
- PCF #1 (LEFT): `0x38` ($A_2=1, A_1=1, A_0=1$)
- PCF #2 (RIGHT): `0x39` ($A_2=1, A_1=1, A_0=0$)

> **Action**: Run `I2C_Scanner.ino` to instantly discover the exact address of both modules on your bench.

---

## 4. QTR-8 Sensor Variant Verification

Before final competition runs, confirm your exact QTR-8 hardware type:

1. **Digital Comparator Clones (Most Common)**:
   - Feature potentiometer threshold adjusters or LM393 comparators.
   - Output clean `HIGH` (3.3V) on black and `LOW` (0V) on white.
   - **Compatible directly with PCF8574 digital read**.
2. **QTR-8A (Analog Output)**:
   - Outputs analog voltage proportional to reflectance.
   - Requires ADC reading or external digital thresholding.
3. **QTR-8RC (RC Pulse Timing)**:
   - Genuine Pololu RC timing version requires pulling I/O lines HIGH for 10µs, setting to input, and timing decay.
   - If using genuine QTR-8RC, digital reading via PCF8574 requires fixed-interval timing or direct microcontroller timing.

---

## 5. Software Architecture & Directory Layout

The workspace is organized into separate diagnostic utilities and the main competition firmware:

```
c:\Users\vishn\Downloads\Varuna 1.0\
├── I2C_Scanner/
│   └── I2C_Scanner.ino        <-- STEP 1: Verify PCF #1 (0x20) and PCF #2 (0x21) addresses
├── Sensor_Test/
│   └── Sensor_Test.ino        <-- STEP 4: 16-Channel physical LEFT -> RIGHT display test
├── Motor_Test/
│   └── Motor_Test.ino         <-- Step 3: BTS7960 motor direction & PWM test
├── Varuna_LineFollower/
│   ├── Config.h               <-- System pins, speeds, PID gains, timing
│   ├── PCF8574_Sensors.h      <-- 16-channel expander header
│   ├── PCF8574_Sensors.cpp    <-- Weighted error & line-loss tracking implementation
│   ├── MotorDriver.h          <-- BTS7960 ESP32 Core 3.x driver header
│   ├── MotorDriver.cpp        <-- 20kHz PWM implementation
│   ├── PIDController.h        <-- PID controller header
│   ├── PIDController.cpp      <-- PID computation with anti-windup & filtering
│   └── Varuna_LineFollower.ino<-- Competition main loop (400 Hz control)
└── HARDWARE_HANDOFF.md        <-- This complete guide
```

---

## 6. Step-by-Step Commissioning & PID Tuning Guide

Follow this sequential workflow on the actual competition track:

### Step 1: Run I2C Scanner
Flash `I2C_Scanner.ino` and open Serial Monitor at 115200 baud. Confirm `0x20` and `0x21` (or `0x38` and `0x39`) appear.

### Step 2: Run Sensor Diagnostic Test
Flash `Sensor_Test.ino`. Move the robot manually across the track line.
- Confirm white background prints `0` for all sensors.
- Confirm black line prints `1` for active sensors.
- Confirm moving line to the left yields negative position error (e.g. `-4500`).
- Confirm moving line to the right yields positive position error (e.g. `+4500`).
- Confirm center alignment yields position error near `0`.

### Step 3: Run Motor Diagnostic Test
Flash `Motor_Test.ino` with the vehicle elevated off the ground.
- Confirm forward command spins both left and right wheels forward.
- If a side spins backward, swap the `RPWM` and `LPWM` pins or motor wire pairs for that side.

### Step 4: PID Tuning on Track
Flash `Varuna_LineFollower.ino`. Start with low base speeds (`SPEED_BASE = 90`) and tune PID gains:
1. **$K_p$ (Proportional)**: Increase $K_p$ from `0.020` until the vehicle follows straight lines cleanly and enters mild curves. If it oscillates wildly (fish-tailing), reduce $K_p$.
2. **$K_d$ (Derivative)**: Increase $K_d$ from `0.100` to dampen oscillations and improve reaction speed on curve entry.
3. **$K_i$ (Integral)**: Keep $K_i$ small (`0.00001` to `0.00005`) to eliminate steady-state offset on gentle sweeping turns.

### Step 5: Speed Calibration
Once PID tracking is stable, incrementally increase speed parameters in `Config.h`:
- `SPEED_STRAIGHT`: `135` $\rightarrow$ `160` $\rightarrow$ `190`
- `SPEED_BASE`: `115` $\rightarrow$ `135` $\rightarrow$ `155`
