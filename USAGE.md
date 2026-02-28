# Usage Guide

> For hardware setup, wiring, flashing, configuration, and architecture, see [README.md](README.md).

---

## First Use

Once the device is built, wired, and flashed:

1. Flip the switch to **REGISTER** (LOW position)
2. Touch the sensor — follow the 3-capture enrollment (the LED ring guides you)
3. Enter your Mac password when prompted (masked with `*`)
4. Confirm the password
5. Flip the switch to **RECOGNIZE**
6. Touch the sensor — watch your Mac lock and unlock

### Re-Registration

To change your password or re-enroll a fingerprint, simply flip back to **REGISTER** and touch the sensor again. The old registration stays intact until the new one is fully committed — no risk of losing your existing setup if something goes wrong mid-process.

---

## Daily Operation

With the switch in **RECOGNIZE** mode, the device is always ready. Touch the sensor, and it will lock your Mac, wake the login screen, type your password, and press Enter. There's a 5-second cooldown between unlock sequences to prevent accidental rapid triggers.

If no fingerprint is registered (or the device is in a corrupt state), the LED ring shows solid red and ignores all touches until you flip to REGISTER.

---

## Web Serial Monitor

A browser-based serial monitor that replaces the Arduino IDE Serial Monitor. No installs needed — just open the page in Chrome or Edge.

**Live:** [https://dattasaurabh82.github.io/diy_fingerprint_based_unlocker/](https://dattasaurabh82.github.io/diy_fingerprint_based_unlocker/)

[![alt text](<assets/Screenshot 2026-02-28 at 17.53.14.png>)](https://dattasaurabh82.github.io/diy_fingerprint_based_unlocker/)


**Local:** `npx serve web/` and open `http://localhost:3000`

### Features

- **Connect/Disconnect** — filters for RP2350 USB VID (0x2E8A), fixed 115200 baud
- **Reset button** — sends `!RESET` to the device, auto-reconnects after reboot
- **Password masking** — input field automatically hides text when the firmware prompts for a password (yellow highlight + lock icon), switches back to plain text afterward
- **Responsive terminal** — xterm.js with Nord dark theme, resizes with the browser window
- **Clear console** — wipes the terminal scrollback

### Requirements

- **Desktop only** — Chrome 89+ or Edge 89+ (Web Serial API)
- **USB connection** — device must be plugged in via USB-C
- Mobile browsers are not supported (a warning is shown)

### Registration via Web Monitor

1. Open the Web Serial Monitor and click **Connect**
2. Select the RP2350 device from the port picker
3. Flip switch to **REGISTER** and touch the sensor
4. Follow the 3-capture enrollment in the terminal
5. When the password prompt appears, the input bar turns yellow with a lock icon — type your password (dots are shown) and press Enter
6. Confirm the password the same way
7. Flip switch to **RECOGNIZE** and touch to unlock

---

## LED Guide

The sensor's built-in RGB LED ring provides visual feedback for every state:

| State | LED | Color |
|-------|-----|-------|
| Boot OK | Breathing | Blue |
| Sensor failed | Solid | Red |
| Register — waiting | Breathing | Yellow |
| Capture success | Fast blink | Green |
| Capture fail | Fast blink | Red |
| Password input | Breathing | Cyan |
| Registration done | Solid | Green |
| Recognize — ready | Breathing | Blue |
| Match found | Solid | Green |
| No match | Fast blink | Red |
| No registration | Solid | Red |
| Cooldown | Slow blink | Green |
| Corrupt state | Fast blink (5x) | Red |

---

## Troubleshooting

> For setup and build issues (wiring, flashing, board config), see [README.md → Troubleshooting](README.md#troubleshooting-setup--build).

<details>
<summary><strong>Device locks Mac but doesn't type password</strong></summary>

The 2-second delays between lock→wake→type are tuned for macOS. If your Mac is slow to show the password field (e.g., external display, FileVault), increase `WAKE_SETTLE_MS` in `config.h`.

</details>

<details>
<summary><strong>Fingerprint not recognized after re-registration</strong></summary>

Make sure you completed all 3 captures during enrollment. If you lifted your finger too quickly on any capture, the enrollment may have stored a poor template. Re-register with firm, centered touches.

</details>

<details>
<summary><strong>LED shows solid red in RECOGNIZE mode</strong></summary>

No valid registration exists. Flip to REGISTER, touch the sensor, and complete the enrollment + password flow.

</details>

<details>
<summary><strong>LED blinks red fast (5x) on boot</strong></summary>

The device detected a corrupt state — mismatched EEPROM and sensor data. It will auto-clear and force REGISTER mode. Just re-register.

</details>

<details>
<summary><strong>Web Serial Monitor won't connect</strong></summary>

Make sure you're using Chrome or Edge on desktop. The Web Serial API is not available in Firefox, Safari, or mobile browsers. If the port picker is empty, check that the device is plugged in and the USB cable supports data (not charge-only).

</details>

<details>
<summary><strong>Web Serial Monitor disconnects twice after reset</strong></summary>

This is expected. The RP2350 re-enumerates USB twice during boot (CDC first, then CDC+HID composite). The monitor handles this automatically with a 6-second stability window — just wait for it to reconnect.

</details>

<details>
<summary><strong>Password masking doesn't activate in Web Monitor</strong></summary>

The password prompt detection looks for `Enter password` and `Confirm password` in the serial stream. If the terminal was cleared before the prompt arrived, the detection may have missed it. Try again without clearing mid-registration.

</details>
