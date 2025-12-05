# PIC24F Pattern Authentication System

A pattern-based authentication system for the **PIC24F Starter Kit 1**, featuring Android-style swipe pattern passwords with real-time visualization on an OLED display.

## Features

- **User Registration & Login** - Multi-user support (up to 10 users)
- **2-Digit User ID** - Numeric ID using capacitive touch buttons (1-5)
- **4-Button Pattern Password** - Swipe-based pattern lock (like Android)
- **Real-Time Pattern Display** - Lines drawn on OLED as you swipe
- **5 Capacitive Touch Buttons** - UP, RIGHT, DOWN, LEFT, CENTER
- **128x64 OLED Display** - Visual feedback for all interactions

## Hardware

- **Board:** PIC24F Starter Kit 1
- **Display:** SH1101A 128x64 OLED
- **Input:** 5 Capacitive Touch Pads
- **LEDs:** RGB LEDs (PWM controlled)

## Button Mapping

```
       1 (UP)
         
   4 ── 5 ── 2    (LEFT - CENTER - RIGHT)
         
       3 (DOWN)
```

| Button  | Number | Position |
|---------|--------|----------|
| UP      | 1      | Top      |
| RIGHT   | 2      | Right    |
| DOWN    | 3      | Bottom   |
| LEFT    | 4      | Left     |
| CENTER  | 5      | Middle   |

## Usage

### Main Menu
- **Press 4 (LEFT)** → Register new user
- **Press 2 (RIGHT)** → Login

### Registration
1. Enter 2-digit User ID (tap two buttons)
2. Draw a 4-button pattern (swipe across buttons)
3. Pattern is saved - Registration complete!

### Login
1. Enter your 2-digit User ID
2. Draw your 4-button pattern
3. If pattern matches → Login successful!

### Pattern Drawing
- Swipe your finger across 4 different buttons
- Lines are drawn on screen as you swipe
- Each button can only be used once per pattern
- Order matters! (1→5→2→3 ≠ 3→2→5→1)

## Project Structure

```
├── main.c           # Main application & authentication logic
├── SH1101A.c/h      # OLED display driver with text & graphics
├── TouchSense.c/h   # Capacitive touch sensor driver
├── RGBLeds.c/h      # RGB LED driver
├── PIC24FStarter.h  # Board configuration
└── README.md        # This file
```

## Building

1. Open project in **MPLAB X IDE**
2. Select PIC24F Starter Kit as target
3. Build project (Clean and Build)
4. Program the device

## Technical Details

### Authentication Storage
- **RAM-based** (data lost on power off)
- Up to 10 users
- User struct: ID (2 bytes) + Pattern (4 bytes) + Active flag (1 byte)

### Touch Detection
- Uses CTMU (Charge Time Measurement Unit)
- Aggregate-based detection for smooth swipe tracking
- Threshold: 6 (detection) / 2 (release)

### Display
- 128x64 monochrome OLED
- 5x7 pixel font (uppercase A-Z, numbers, symbols)
- Bresenham's line algorithm for pattern drawing

## Credits

- **Original Hardware Drivers:** kvl@eti.uni-siegen.de
- **Authentication System:** [AdityaDk10](https://github.com/AdityaDk10)

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
