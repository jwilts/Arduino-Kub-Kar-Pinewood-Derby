# 🏁 Arduino Pinewood Derby (Cub Car) Timer

An inexpensive, stand-alone Pinewood Derby race timer built using an
**Arduino Nano**.

Designed to be simple, reliable, and easy to operate --- no PC required
during races.

------------------------------------------------------------------------

## 📌 Overview

This timer system:

-   Supports **3 or 4 lanes** (compile-time selection)
-   Displays **finishing order via LED bank**
-   Shows **race times and instructions on a 20x4 I2C LCD**
-   Uses **analog IR break-beam sensors** for accurate finish detection
-   Operates completely standalone (no computer required during races)

Originally inspired by Raspberry Pi systems, this Arduino version
dramatically reduces cost and complexity while maintaining reliability.

------------------------------------------------------------------------

## 🎯 Why Arduino Instead of Raspberry Pi?

Raspberry Pi timers can:

-   Store results in databases
-   Run GUI applications
-   Use RFID tracking
-   Provide advanced automation

But they:

-   Are expensive (\~\$100+ for a Pi 4)
-   Require OS setup and configuration
-   Require Linux knowledge to operate

This Arduino-based timer:

-   Costs under **\$45**
-   Requires **no OS configuration**
-   Is easy to operate by volunteers
-   Boots directly into race mode

------------------------------------------------------------------------

# 🚦 Features

-   🏎 Supports 3 or 4 lanes
-   🟢 LED bank indicates finishing order
-   📟 20x4 I2C LCD shows times and instructions
-   ⏱ 15-second race timeout (DNF after timeout)
-   🔧 Automatic startup calibration
-   🔍 Live IR signal strength display during alignment
-   🧪 Debug mode via Serial Monitor
-   🔌 Standalone operation

------------------------------------------------------------------------

# 🔢 Lane Configuration

Set the number of lanes at compile time:

``` cpp
#define LANE_COUNT 3   // or 4
```

------------------------------------------------------------------------

# 👁 Sensor System

## IR Break-Beam (Analog -- Recommended)

The current version uses **analog IR break-beam sensors**, which are
significantly more reliable than LDR shadow sensors.

### How It Works

-   Each lane has:
    -   An IR emitter (LED)
    -   An IR receiver
-   Beam crosses the track
-   When a car breaks the beam, the analog value changes
-   System auto-calibrates threshold and polarity

### Startup Alignment Screen

On power-up:

1.  LCD displays signal strength bars per lane
2.  Adjust emitters for strongest signal
3.  Press Timer Button to proceed
4.  System captures:
    -   Baseline (beam clear)
    -   Blocked (car interrupting beam)
5.  Thresholds are calculated automatically

------------------------------------------------------------------------

# 🔌 Wiring (IR -- Analog Only)

## IR Emitters

Arduino Digital Pin → 300Ω resistor → IR LED Anode (Long Leg)\
IR LED Cathode (Short Leg / Flat Side) → GND

Emitters are **ACTIVE HIGH**:

``` cpp
digitalWrite(pin, HIGH);  // Beam ON
```

### Emitter Pins

  Lane   Pin
  ------ ---------------
  1      D4
  2      D5
  3      D9
  4      D7 (optional)

------------------------------------------------------------------------

## IR Receivers (Analog)

3V3 → IR Receiver Anode (Long Leg)\
IR Receiver Cathode → Arduino Analog Pin → 10kΩ resistor → GND

### Receiver Pins

  Lane   Pin
  ------ ---------------
  1      A0
  2      A2
  3      A1
  4      A3 (optional)

------------------------------------------------------------------------

# 🖥 Display & LED System

## LCD

-   20x4 I2C LCD
-   Displays calibration steps, race times, and reset prompts

## LED Bank

Two options:

### Option 1 -- 74HC595 Shift Registers

-   2 × 74HC595
-   9 LEDs (3 lane)
-   16 LEDs (4 lane)
-   330Ω resistor per LED

Pins used:

  Nano Pin   Function
  ---------- ----------
  D8         Latch
  D11        Data
  D12        Clock

------------------------------------------------------------------------

### Option 2 -- WS2812B LED Strip

-   Fewer components
-   Programmable colors and patterns
-   Requires careful installation

------------------------------------------------------------------------

# 🚀 Startup Process

After power-up:

1.  LED test sequence runs
2.  IR alignment screen appears
3.  Press Timer Button
4.  Baseline captured (no cars)
5.  Place cars shading sensors
6.  Press Timer Button again
7.  Thresholds calculated
8.  Ready to race

------------------------------------------------------------------------

# 🏁 Running a Race

## Start

-   Close starting gate
-   Release gate
-   Timers start automatically

## During Race

-   Times display live as cars finish
-   LEDs indicate finishing order

## End of Race

After all cars finish (or after 15-second timeout):

Push Timer Button

Press Timer Button to reset for the next race.

⚠️ Do not close the start gate until instructed on the screen.

------------------------------------------------------------------------

# 🔋 Power Requirements

Recommended:

-   7--12V DC input
-   Buck converter for stable 5V + 3.3V
-   Avoid weak USB power bricks

------------------------------------------------------------------------

# 📜 Version

**Version 2.0**\
IR Analog Break-Beam Conversion\
Compile-Time Lane Selection\
March 2026

Author: Jeff Wilts
