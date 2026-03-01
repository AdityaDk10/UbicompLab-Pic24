# PIC24F Pattern Authentication System

A pattern-based authentication system for the **PIC24F Starter Kit 1**, featuring Android-style swipe pattern passwords with real-time visualization on an OLED display and **Flash‑backed persistent user storage**.

## Features

- **User Registration, Login, Delete, List** – Multi-user support (up to **25 users**).
- **Persistent Flash Storage** – User database and deleted‑user history are stored in program Flash and **survive resets/power‑off**.
- **2-Digit User ID** – Numeric ID entered using the 5 capacitive touch buttons (digits 1–5; current input scheme effectively supports 25 unique IDs).
- **5-Button Pattern Password** – Swipe-based pattern lock (Android-style) using a fixed 5‑button pattern.
- **Timing-Based Matching** – Inter-button timing is recorded and used to give feedback on how close the login timing is to the registered pattern.
- **Account Lockout & Unlock** – Accounts are locked after 3 failed attempts; an admin LIST menu includes a locked‑user browser and unlock action.
- **Deleted User History** – Recently deleted user IDs (up to 10) are tracked and displayed in the LIST menu, persisted in Flash.
- **Improved Menus & UI** – Two‑screen main menu (REGISTER/LOGIN and DELETE/LIST) with arrow + underline selection, plus a multi‑screen LIST submenu (REGISTERED, ACTIVE USERS, LOCKED, DELETED, BACK).
- **Real-Time Pattern Display** – Lines drawn on the OLED as you swipe through the buttons.
- **5 Capacitive Touch Buttons** – UP, RIGHT, DOWN, LEFT, CENTER.
- **128x64 OLED Display** – Visual feedback for all interactions.

## Hardware

- **Board:** PIC24F Starter Kit 1
- **Display:** SH1101A 128x64 OLED
- **Input:** 5 Capacitive Touch Pads
- **LEDs:** RGB LEDs (PWM controlled)

## Button Mapping

```text
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

### Main Menu (Top-Level)

The main menu has **two screens**, each with two side‑by‑side options:

- **Screen 0:** `REGISTER` (left) | `LOGIN` (right)
- **Screen 1:** `DELETE` (left) | `LIST` (right)

Navigation (as implemented in `main.c`):

- **UP (1):** Switch to **Screen 0** (REGISTER/LOGIN).
- **DOWN (3):** Switch to **Screen 1** (DELETE/LIST).
- **LEFT (4):** Select the **left** option on the current screen.
- **RIGHT (2):** Select the **right** option on the current screen.
- **CENTER (5):** Confirm the currently selected option.

The selected item is indicated by:

- An **arrow (`>`)** drawn just to the left of the selected label.
- An **underline** drawn under the selected label.
- A vertical line in the middle of the screen separating the left/right options.

### Registration Flow

1. From the main menu, select **REGISTER** (Screen 0 → left option).
2. Enter a **2-digit User ID** using the capacitive buttons (digits 1–5).
3. Draw a **5-button pattern** (each button used at most once).
4. The pattern + timing are saved to the **Flash‑backed database** if:
   - The database is not full (limit: 25 users).
   - The User ID does not already exist.

### Login Flow

1. From the main menu, select **LOGIN** (Screen 0 → right option).
2. Enter your **2-digit User ID**.
3. Draw your **5-button pattern**.
4. The system:
   - Verifies the ID exists.
   - Compares both pattern and timing.
   - Marks the user as **logged in** on success.
   - Increments the failed‑attempt counter on failure.
   - **Locks the account** after 3 failed attempts.

### Delete User

1. From the main menu, select **DELETE** (Screen 1 → left option).
2. Enter the **2-digit User ID** to delete.
3. If the user exists, their slot is cleared and:
   - The ID is appended to the **deleted‑user history** (ring buffer, max 10 entries).
   - The updated database and history are written to Flash.

### LIST Menu (Admin)

1. From the main menu, select **LIST** (Screen 1 → right option).
2. Enter the **admin password** (currently hardcoded to `1111`).
3. Navigate the LIST submenu:
   - **Screen 0:** `REGISTERED`, `ACTIVE USERS`, `LOCKED`.
   - **Screen 1:** `DELETED`, `BACK`.
4. The selected item is highlighted with arrow + underline (same style as main menu).

Sub-pages:

- **REGISTERED:** Shows all active user IDs.
- **ACTIVE USERS:** Shows users that are currently logged in.
- **LOCKED:** Shows users locked by too many failures; supports **scrolling** and selecting a user to unlock.
- **DELETED:** Shows up to the **10 most recently deleted IDs** (from Flash‑backed history).
- **BACK:** Returns to the top‑level main menu.

## Project Structure

```text
├── main.c           # Main application, UI, and authentication logic
├── SH1101A.c/h      # OLED display driver with text & graphics
├── TouchSense.c/h   # Capacitive touch sensor driver (CTMU + ADC)
├── RGBLeds.c/h      # RGB LED driver
├── PIC24FStarter.h  # Board configuration
└── README.md        # This file
```

## Building

1. Open the project in **MPLAB X IDE**.
2. Select the **PIC24F Starter Kit 1** as the target.
3. Build the project (Clean and Build).
4. Program the device.

## Technical Details

### Authentication Storage (Current)

- **Flash‑based**: User database and deleted‑ID history are stored in program Flash at a dedicated page (see `FLASH_PAGE_ADDR` in `main.c`).
- **Capacity:** Up to **25** users (`MAX_USERS`).
- Data stored per user (see `User` struct in `main.c`):
  - 2‑byte User ID.
  - 5‑button pattern (`PATTERN_LENGTH = 5`).
  - Inter‑button timing array (`PATTERN_LENGTH - 1` entries).
  - Active flag, failed‑attempt counter, logged‑in flag.
- **Deleted user history:** An array of up to 10 most recently deleted IDs, also persisted in Flash.
- A **valid‑flag** is written to Flash so the firmware can detect uninitialized/invalid pages and fall back to an empty database on first boot.

> Note: An earlier version was **RAM‑only** with a 10‑user limit. The current codebase uses Flash for persistence and a 25‑user limit.

### Touch Detection

- Uses the **CTMU (Charge Time Measurement Unit)** with the ADC to sense capacitive touch.
- Aggregate‑based detection for smooth swipe tracking across buttons.
- Thresholds (as used in `main.c`):
  - Detection threshold around **6**.
  - Release threshold around **2** for digit entry.

### Display

- 128x64 monochrome OLED (SH1101A).
- 5x7 pixel font (uppercase A–Z, numbers, symbols).
- Bresenham's line algorithm for pattern drawing.
- Custom UI code for:
  - Two‑column main menu with arrow + underline highlight.
  - Multi‑page LIST submenu.
  - Animated loading/success/error messages.

## Credits

- **Original Hardware Drivers:** kvl@eti.uni-siegen.de
- **Authentication System:** [AdityaDk10](https://github.com/AdityaDk10), Gayathri Ragupathi

## License

This project is licensed under the GNU General Public License v3.0 – see the `LICENSE` file for details.
