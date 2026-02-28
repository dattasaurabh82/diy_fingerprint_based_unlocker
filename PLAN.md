# DIY Fingerprint Unlocker — Full Implementation Plan

## Hardware Summary
- **MCU**: Waveshare RP2350-Zero (RP2350A, 3.3V, USB-C, 4MB flash)
- **Sensor**: DFRobot SEN0348 Capacitive Fingerprint (UART, 115200 baud)
- **Switch**: SPDT on GPIO3 (LOW=GND=REGISTER, HIGH=pullup=RECOGNIZE)
- **UART**: Serial1 → GPIO0(TX)→Sensor RX, GPIO1(RX)→Sensor TX
- **IRQ**: GPIO2 (available but unused in v1 — polling detectFinger() instead)
- **HID**: USB Keyboard via Keyboard.h (arduino-pico core)

## Pin Map
```
GPIO0  — UART0 TX → Sensor RX (Black wire)
GPIO1  — UART0 RX ← Sensor TX (Yellow wire)
GPIO2  — IRQ (reserved, unused v1)
GPIO3  — SPDT Switch (other leg to GND)
3V3    — Sensor VCC (Green wire) + VIN (White wire)
GND    — Sensor GND (Red wire) + Switch common
```

## EEPROM Layout (v1)
```
Address  Size   Contents
0x00     1      Magic byte (0xA5 = valid registration exists)
0x01     1      Active slot ID (1 or 2 — which sensor slot holds the live fingerprint)
0x02     1      Password length (0-32)
0x03     32     Password chars (null-padded)
0x23     1      Checksum (XOR of bytes 0x00-0x22)
─────────────
Total: 36 bytes of 4096 available
```
Magic byte + checksum = double validation that EEPROM contents are intentional, not garbage from virgin flash.

## Fingerprint Storage — Two-Slot Alternating Strategy
Sensor has **80 slots**. We use exactly **two** (ID 1 and ID 2) in an alternating pattern.

**Why**: If we delete the old fingerprint before the new registration completes, and
something fails mid-registration (timeout, bad password, switch flip, power loss),
the old fingerprint is gone forever — the sensor doesn't expose raw templates for
backup/restore. By enrolling the NEW fingerprint to the OTHER slot first, the old
slot stays intact until the entire atomic commit succeeds.

```
EEPROM.activeSlot = 1  (current live fingerprint is in sensor slot 1)

New registration starts:
  → Enroll new fingerprint to slot 2 (the "staging" slot)
  → Collect password via serial
  → Confirm password
  → Write EEPROM: magic + activeSlot=2 + password + checksum
  → EEPROM.commit() + verify
  → Delete old slot 1   ← only NOW is it safe to delete
  → Done: activeSlot is now 2

Next registration:
  → Enroll to slot 1 (staging)
  → ...success...
  → Write EEPROM: activeSlot=1 + password
  → Delete old slot 2
  → Done: activeSlot is now 1
```

**On failure at any point**: delete the staging slot, old registration untouched.
**On boot**: validate that EEPROM.activeSlot matches an actually-enrolled slot.
Clean up any orphaned staging slot from a previous interrupted registration.

---

## STATE MACHINE

```
                    ┌──────────┐
                    │   BOOT   │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │ VALIDATE │──── corrupt? ──→ FORCE_REGISTER
                    └────┬─────┘
                         │ valid or virgin
                    ┌────▼──────┐
                    │ READ      │
                    │ SWITCH    │
                    └──┬─────┬──┘
                       │     │
              LOW(GND) │     │ HIGH(pullup)
                       │     │
                  ┌────▼──┐ ┌▼───────┐
                  │REGISTER│ │RECOGNIZE│
                  └────────┘ └────────┘
```

### States Detail

**BOOT**
1. Init Serial (115200, 5s timeout for USB CDC)
2. Init GPIO3 (INPUT_PULLUP)
3. Init Serial1 (115200) → fingerprint.begin()
4. If sensor init fails → red LED, halt with error
5. Print firmware version, device info
6. → VALIDATE

**VALIDATE (boot integrity check)**
1. Read EEPROM: magic byte, activeSlot, password, checksum
2. Check sensor: which slots (1, 2) have fingerprints enrolled
3. Decision matrix:
   - EEPROM valid (magic=0xA5, checksum OK) AND activeSlot's fingerprint enrolled → **VALID**
     - Clean up: if OTHER slot has an orphan fingerprint (from interrupted reg), delete it
     - Serial: `[BOOT] Valid registration in slot N`
   - EEPROM invalid AND no fingerprints in slot 1 or 2 → **VIRGIN**
     - Serial: `[BOOT] Virgin device`
   - EEPROM valid BUT activeSlot's fingerprint missing → **CORRUPT**
     - Clear EEPROM, delete any enrolled fingerprints
     - Serial: `[WARNING] Fingerprint missing for active slot — cleared`
   - EEPROM invalid BUT fingerprint(s) exist → **CORRUPT**
     - Delete all enrolled fingerprints, EEPROM already invalid
     - Serial: `[WARNING] Orphan fingerprint(s) without password — cleared`
4. VIRGIN → force REGISTER regardless of switch position
5. VALID → read switch, go to REGISTER or RECOGNIZE

**REGISTER MODE**
- LED: Breathing Yellow (waiting)
- Serial: `[MODE] REGISTER`
- Flow:
  1. Prompt: "Place finger (1/3)..."
  2. Delete existing slot 1 if present (cleanup before fresh enrollment)
  3. Collect fingerprint ×3 with lift-between detection
  4. Store to slot 1
  5. Prompt: "Enter password via Serial (max 32 chars):"
  6. Read password with masked echo (prints `*` per char)
  7. Prompt: "Confirm password:"
  8. Compare → mismatch? → retry (max 3 attempts) → rollback fingerprint
  9. Write password to EEPROM (magic + length + chars + checksum)
  10. EEPROM.commit()
  11. Verify: re-read EEPROM, validate checksum
  12. LED: Solid Green (2s) → success
  13. Serial: `[REG] Registration complete`
  14. Return to switch-reading idle

**RECOGNIZE MODE**
- LED: Breathing Blue (ready)
- Serial: `[MODE] RECOGNIZE`
- Flow:
  1. Wait for finger (detectFinger() polling)
  2. Capture fingerprint
  3. Search (match against slot 1)
  4. NO MATCH → Red blink, `[AUTH] No match`, return to waiting
  5. MATCH → Green solid
  6. Execute HID unlock sequence:
     a. Lock screen (Ctrl+Cmd+Q) — safe if already locked/sleeping
     b. Wake (LEFT_CTRL × 2 — non-printable, won't type into password field)
     c. Clear password field (Cmd+A selects all, so first typed char replaces)
     d. Type password from EEPROM
     e. Press Enter
  7. LED: Green solid (2s) → back to breathing blue
  8. 5-second cooldown (ignore touches)
  9. Serial: `[AUTH] Unlock sequence sent`

---

## FLOW DETAILS WITH EDGE CASES

### Registration Flow (detailed) — Two-Slot Safe Registration

**Key principle**: Never destroy old registration data until new registration is
fully committed and verified. The old fingerprint+password pair remains intact
throughout the entire process. Only deleted after atomic commit succeeds.

```
REGISTER entered
    │
    ├── Determine slots:
    │   ├── Read activeSlot from EEPROM (1 or 2)
    │   ├── stagingSlot = (activeSlot == 1) ? 2 : 1
    │   ├── If virgin (no activeSlot): activeSlot=0, stagingSlot=1
    │   └── Serial: "[REG] Active slot: N, staging to slot: M"
    │
    ├── Switch check every loop iteration
    │   └── Switch changed? → ABORT → delete staging slot only → go to new mode
    │
    ├── Step 1: Clean staging slot (not the active one!)
    │   ├── delFingerprint(stagingSlot) — remove any orphan from previous failed attempt
    │   └── Serial: "[REG] Cleaned staging slot M"
    │   NOTE: Active slot is UNTOUCHED. Old registration is fully intact.
    │
    ├── Step 2: Fingerprint enrollment to STAGING slot
    │   │
    │   ├── For each capture (i = 1,2,3):
    │   │   ├── LED: Breathing Yellow
    │   │   ├── Serial: "[REG] Place finger (i/3)..."
    │   │   ├── collectionFingerprint(10) — 10s timeout
    │   │   │   ├── Success → LED: Flash Green, Serial: "[REG] Captured i/3"
    │   │   │   │   ├── Wait for finger lift: while(detectFinger()) delay(100)
    │   │   │   │   └── Serial: "[REG] Remove finger..."
    │   │   │   └── Timeout/Fail → LED: Flash Red
    │   │   │       ├── Serial: "[REG] Capture failed, retry"
    │   │   │       └── Retry same step (max 3 retries per step)
    │   │   └── 3 retries exhausted for any step:
    │   │       ├── delFingerprint(stagingSlot) ← clean up staging only
    │   │       ├── Old registration UNTOUCHED
    │   │       └── Serial: "[REG] Enrollment failed — old registration preserved"
    │   │
    │   └── storeFingerprint(stagingSlot)  ← stores to staging, NOT active
    │       ├── Success → continue to password
    │       └── Failure:
    │           ├── delFingerprint(stagingSlot)
    │           └── Serial: "[REG] Store failed — old registration preserved"
    │
    ├── Step 3: Password input via Serial
    │   ├── LED: Breathing Cyan
    │   ├── Serial: "[REG] Enter password (max 32 chars, Enter to confirm):"
    │   ├── Read chars one by one, echo '*' for each
    │   ├── Enter/newline → end input
    │   ├── Timeout: 30 seconds of no input:
    │   │   ├── delFingerprint(stagingSlot) ← clean staging only
    │   │   └── Serial: "[REG] Timeout — old registration preserved"
    │   ├── Empty password → reject, re-prompt
    │   │
    │   ├── Serial: "[REG] Confirm password:"
    │   ├── Read again with masked echo
    │   ├── Compare:
    │   │   ├── Match → proceed to commit
    │   │   └── Mismatch → Serial: "[REG] Mismatch! Try again (attempt N/3)"
    │   │       └── 3 mismatches:
    │   │           ├── delFingerprint(stagingSlot)
    │   │           └── Serial: "[REG] Too many mismatches — old registration preserved"
    │   │
    │   └── Timeout during confirm:
    │       ├── delFingerprint(stagingSlot)
    │       └── Serial: "[REG] Timeout — old registration preserved"
    │
    ├── Step 4: Atomic commit (the ONLY step that changes live state)
    │   ├── Write EEPROM: magic(0xA5) + activeSlot=stagingSlot + password + checksum
    │   ├── EEPROM.commit()
    │   ├── Verify: re-read and validate checksum
    │   │   ├── Valid → SUCCESS
    │   │   │   ├── Delete OLD slot (the previous activeSlot) — safe now
    │   │   │   ├── LED: Solid Green (2s)
    │   │   │   ├── Serial: "[REG] ✓ Registration complete (slot M now active)"
    │   │   │   └── Return to idle
    │   │   └── Invalid → EEPROM write failed
    │   │       ├── Rollback: delFingerprint(stagingSlot) + restore old EEPROM
    │   │       │   (old EEPROM data still intact since we only wrote new data)
    │   │       │   Re-write: magic + old activeSlot + old password + checksum
    │   │       │   (We keep old password in RAM until commit is verified)
    │   │       ├── LED: Fast Red blink
    │   │       └── Serial: "[REG] ✗ EEPROM verify failed — old registration restored"
    │
    └── ROLLBACK (on any failure):
        ├── delFingerprint(stagingSlot) ← ONLY the staging slot
        ├── EEPROM is UNCHANGED (old registration data still there)
        ├── Old fingerprint in old activeSlot still works
        └── Serial: "[REG] Rolled back — old registration preserved"

    WHAT'S PRESERVED ON FAILURE:
    ├── Old fingerprint in sensor slot [activeSlot] → still matches
    ├── Old password in EEPROM → still readable
    └── Device continues to work in RECOGNIZE mode with old credentials
```

### Recognition Flow (detailed)

```
RECOGNIZE entered
    │
    ├── Boot validation passed? (fingerprint + password both exist)
    │   └── No → LED: Solid Red, Serial: "[AUTH] No registration found"
    │       └── Ignore all touches until switch flipped to REGISTER
    │
    ├── LED: Breathing Blue (ready)
    ├── Serial: "[AUTH] Ready — touch sensor to unlock"
    │
    ├── Poll loop:
    │   ├── Check switch every iteration → changed? → exit to new mode
    │   ├── In cooldown? → skip touch detection
    │   ├── detectFinger()?
    │   │   ├── No → continue polling
    │   │   └── Yes:
    │   │       ├── collectionFingerprint(5) — 5s timeout
    │   │       │   └── Fail → Serial: "[AUTH] Capture failed", continue
    │   │       ├── search()
    │   │       │   ├── No match (0 or ERR_ID809):
    │   │       │   │   ├── LED: Fast Red blink (3 cycles)
    │   │       │   │   ├── Serial: "[AUTH] ✗ No match"
    │   │       │   │   └── Wait for finger lift → continue
    │   │       │   │
    │   │       │   └── Match (returns ID 1):
    │   │       │       ├── LED: Solid Green
    │   │       │       ├── Serial: "[AUTH] ✓ Match — sending unlock"
    │   │       │       │
    │   │       │       ├── HID Sequence:
    │   │       │       │   ├── Read password from EEPROM
    │   │       │       │   ├── Step 1: Ctrl+Cmd+Q (lock) → 2s wait
    │   │       │       │   │   Safe if already locked or sleeping — no effect
    │   │       │       │   ├── Step 2: LEFT_CTRL press+release ×2 (wake) → 2s wait
    │   │       │       │   │   Non-printable modifier — wakes display without
    │   │       │       │   │   typing any character into the password field
    │   │       │       │   │   Safe if already awake — no visible effect
    │   │       │       │   ├── Step 3: Cmd+A (select-all in password field) → 200ms
    │   │       │       │   │   Selects any stale/junk chars in the field
    │   │       │       │   │   First typed char of password will replace selection
    │   │       │       │   │   Safe if field is empty — no-op
    │   │       │       │   ├── Step 4: Type password → 100ms wait
    │   │       │       │   │   First char overwrites any selection from Step 3
    │   │       │       │   └── Step 5: Enter → 500ms wait
    │   │       │       │
    │   │       │       ├── Serial: "[AUTH] Unlock sequence complete"
    │   │       │       ├── Start 5s cooldown timer
    │   │       │       └── LED: Green (2s) → back to Breathing Blue
    │   │       │
    │   │       └── Wait for finger lift before next detection
```

---

## LED STATE MAP

| State                    | LED Mode          | Color   | Count |
|--------------------------|-------------------|---------|-------|
| Boot OK                  | Breathing         | Blue    | 3     |
| Sensor init failed       | Keeps On          | Red     | 0     |
| REGISTER idle/waiting    | Breathing         | Yellow  | 0     |
| Waiting for finger place | Breathing         | Yellow  | 0     |
| Finger captured OK       | Fast Blink        | Green   | 3     |
| Finger capture failed    | Fast Blink        | Red     | 3     |
| Waiting for password     | Breathing         | Cyan    | 0     |
| Registration SUCCESS     | Keeps On          | Green   | 0     |
| Registration FAILED      | Fast Blink        | Red     | 3     |
| RECOGNIZE ready          | Breathing         | Blue    | 0     |
| Match found              | Keeps On          | Green   | 0     |
| No match                 | Fast Blink        | Red     | 3     |
| No registration exists   | Keeps On          | Red     | 0     |
| Cooldown active          | Slow Blink        | Green   | 0     |
| Switch change detected   | Fast Blink        | Purple  | 3     |
| Corrupt state warning    | Fast Blink        | Red     | 5     |
| EEPROM verify failed     | Fast Blink        | Red     | 5     |

---

## SERIAL OUTPUT PROTOCOL

Structured for future Web Serial parsing. Format: `[TAG] message`

### Tags:
```
[BOOT]    — startup messages
[SWITCH]  — mode switch changes
[MODE]    — current operating mode
[REG]     — registration flow messages
[AUTH]    — recognition/authentication messages
[HID]     — HID keyboard action messages
[WARNING] — non-fatal warnings
[ERROR]   — fatal errors
[DEBUG]   — verbose debug (can be toggled with #define)
```

### Key Serial Messages:
```
[BOOT] Firmware v1.0
[BOOT] Sensor OK | FAIL
[BOOT] EEPROM: valid | virgin | CORRUPT → cleared
[BOOT] Enrolled: N fingerprint(s)
[BOOT] Ready

[SWITCH] REGISTER | RECOGNIZE

[MODE] REGISTER
[REG] Active slot: N, staging to slot: M
[REG] Cleaned staging slot M
[REG] Place finger (1/3)...
[REG] ✓ Captured 1/3
[REG] Remove finger...
[REG] ✗ Capture failed — retry
[REG] ✗ Max retries — aborted
[REG] Stored to staging slot M
[REG] Enter password (max 32 chars):
[REG] Confirm password:
[REG] ✗ Mismatch (attempt 1/3)
[REG] ✗ Password timeout — rolled back
[REG] Committing...
[REG] ✓ Registration complete
[REG] ✗ EEPROM verify failed — rolled back

[MODE] RECOGNIZE
[AUTH] Ready — touch to unlock
[AUTH] ✗ No registration — flip to REGISTER
[AUTH] ✓ Match
[AUTH] ✗ No match
[AUTH] Sending unlock sequence...
[HID] Lock (Ctrl+Cmd+Q)
[HID] Wake (LEFT_CTRL ×2)
[HID] Clear field (Cmd+A)
[HID] Typing password...
[HID] Enter
[AUTH] ✓ Unlock complete
[AUTH] Cooldown 5s...

[WARNING] Inconsistent state — cleared
[WARNING] Switch changed — aborting operation

[ERROR] Sensor init failed — halting
```

---

## SWITCH DEBOUNCE

```cpp
#define DEBOUNCE_MS 50

int readSwitch() {
    static int lastReading = -1;
    static unsigned long lastChangeTime = 0;
    static int stableState = -1;

    int reading = digitalRead(PIN_MODE_SWITCH);
    if (reading != lastReading) {
        lastChangeTime = millis();
        lastReading = reading;
    }
    if ((millis() - lastChangeTime) > DEBOUNCE_MS && reading != stableState) {
        stableState = reading;
    }
    return stableState;
}
```

---

## SWITCH-CHANGE ABORT MECHANISM

Every long-running operation (fingerprint capture, serial password read) must periodically check the switch state. If it changes mid-operation:

1. Set abort flag
2. If fingerprint was stored to staging slot → `delFingerprint(stagingSlot)`
3. EEPROM is untouched (old registration preserved)
4. LED: Purple fast blink
5. Serial: `[WARNING] Switch changed — aborting operation`
6. Transition to the new mode
7. Old registration (if any) remains fully functional

Implementation: pass a `checkAbort()` function that returns true if switch changed. Call it:
- Inside fingerprint capture wait loops
- Between serial read characters
- Between enrollment steps

---

## EEPROM HELPER FUNCTIONS

```
writeRegistration(activeSlot, password, length) → bool:
    EEPROM.write(0x00, 0xA5)          // magic
    EEPROM.write(0x01, activeSlot)     // which sensor slot (1 or 2)
    EEPROM.write(0x02, length)         // password length
    for i in 0..31: EEPROM.write(0x03+i, password[i] or 0x00)
    checksum = XOR of bytes 0x00..0x22
    EEPROM.write(0x23, checksum)
    EEPROM.commit()
    // verify by re-reading
    return verifyRegistration()

readRegistration(activeSlot_out, password_out, length_out) → bool:
    if EEPROM.read(0x00) != 0xA5 → return false
    activeSlot = EEPROM.read(0x01)
    if activeSlot != 1 && activeSlot != 2 → return false
    length = EEPROM.read(0x02)
    if length == 0 || length > 32 → return false
    read password bytes from 0x03..0x22
    verify checksum at 0x23
    return checksum_valid

clearRegistration():
    EEPROM.write(0x00, 0x00)  // clear magic
    EEPROM.commit()

getActiveSlot() → uint8_t:
    if readRegistration succeeds → return activeSlot
    else → return 0 (no active slot = virgin)

getStagingSlot() → uint8_t:
    active = getActiveSlot()
    if active == 1 → return 2
    if active == 2 → return 1
    return 1  // virgin: first registration goes to slot 1

hasValidRegistration() → bool:
    active = getActiveSlot()
    if active == 0 → return false
    // also check sensor has fingerprint in that slot
    return readRegistration succeeds AND fingerprint enrolled in activeSlot
```

---

## MILESTONE BREAKDOWN

### Milestone 1 — Boot + Switch + LED
- Serial init with timeout
- GPIO3 switch with debounce
- Sensor init + LED breathing blue on success / solid red on fail
- Switch state printed on change
- LED reflects current mode: yellow=REGISTER, blue=RECOGNIZE
- **Test**: flip switch, observe serial + LED changes

### Milestone 2 — Fingerprint sensor init + finger detection
- detectFinger() polling in loop
- LED changes on touch detection (within current mode context)
- Serial prints touch events
- Switch-change still detected during polling
- **Test**: touch sensor in both modes, verify serial output

### Milestone 3 — Registration flow (two-slot safe)
- Two-slot alternating: enroll to staging slot, preserve active slot
- Full enrollment (3× capture + store to staging slot)
- Serial password input with masked echo
- Password confirm with mismatch retry
- EEPROM write (with activeSlot field) + verify
- Delete old active slot only after verified commit
- Rollback = delete staging slot only, old registration untouched
- Switch-change abort (staging slot cleanup)
- Timeouts (10s per capture, 30s for password)
- **Test**: full registration, re-registration (verify old preserved on failure), test rollback

### Milestone 4 — Recognition flow + HID
- FIRST: Add test sketch for modifier keys (Ctrl+Cmd+Q) — `tests/8_hidModifierTest/`
- Match against active slot (read from EEPROM)
- HID unlock sequence: LEFT_CTRL wake + Cmd+A clear + type + Enter
- 5s cooldown
- No-registration guard
- **Test**: register → flip to RECOGNIZE → touch → verify Mac locks then unlocks

### Milestone 5 — Boot validation + atomic safety
- Boot integrity check: EEPROM activeSlot vs actual sensor enrollments
- Orphan staging slot cleanup (from interrupted registration)
- Corrupt state detection (EEPROM says slot 1 but slot 1 empty)
- Power-loss recovery for all failure points
- Full integration testing of all edge cases
- **Test**: simulate corrupt states, orphan slots, power-loss scenarios

### Pre-Milestone 4 test — HID modifier keys
- New test sketch: tests/8_hidModifierTest/
- Tests Ctrl+Cmd+Q combo specifically
- Tests press/releaseAll timing
- Must pass before building Milestone 4

---

## ATTACK SCENARIO VALIDATION

| # | Scenario | Handling | LED | Serial |
|---|----------|----------|-----|--------|
| 1 | Attacker enrolls their finger | Must also set password. Device types attacker's password → Mac rejects | Yellow→Green→Cyan | Full reg flow |
| 2 | Attacker overwrites registration | Victim's print no longer matches. Victim re-registers | Red blink | [AUTH] No match |
| 3 | Power loss mid-registration | New fingerprint was in staging slot. Boot sees orphan staging slot → deletes it. Old registration in active slot is untouched. Device works normally. | Blue breathing | [BOOT] Cleaned orphan staging slot |
| 4 | Serial disconnect mid-password | 30s timeout → delete staging slot only → old registration preserved | Red blink | [REG] Timeout — old preserved |
| 5 | Switch flip mid-registration | Abort → delete staging slot only → old registration preserved | Purple blink | [WARNING] Switch changed |
| 6 | Rapid touches in RUN mode | 5s cooldown ignores | Green→Blue | [AUTH] Cooldown |
| 7 | RUN mode, nothing enrolled | Refuses to act | Solid Red | [AUTH] No registration |
| 8 | Serial not connected in REGISTER | Password timeout → rollback | Red blink | (not visible) |
| 9 | Multiple password mismatches | 3 attempts then rollback | Red blink | [REG] Mismatch |
| 10 | EEPROM write corruption | Verify fails → rollback fingerprint | Red blink | [REG] EEPROM verify failed |

---

## FILE STRUCTURE — Header-Only Modules

```
diy_fingerprint_based_unlocker/
├── diy_fingerprint_based_unlocker.ino   ← main: setup(), loop(), state machine
├── config.h                             ← all #defines, pin map, timing constants
├── switch_control.h                     ← debounce, readSwitch(), checkAbort()
├── eeprom_storage.h                     ← EEPROM read/write/verify/clear helpers
├── led_feedback.h                       ← LED semantic state wrappers
├── registration.h                       ← full registration flow (M3)
├── recognition.h                        ← match + HID unlock flow (M4)
├── hid_unlock.h                         ← HID keyboard sequence (M4)
├── validation.h                         ← boot integrity check (M5)
├── tests/
│   ├── 1_queryDeviceBPS/
│   ├── 2_queryDeviceFullInfo/
│   ├── 3_ledRingTest/
│   ├── 4_fingerTouchDetection/
│   ├── 5_fingerPrintEnrollment/
│   ├── 6_fingerPrintSearchMatch/
│   ├── 7_hidTest/
│   └── 8_hidModifierTest/              ← NEW: test modifier combos
├── PLAN.md
├── CONTEXT.md                           ← compressed knowledge after milestones
├── README.md
└── assets/
```

Modules introduced per milestone, not all at once.
Each .h/.cpp pair owns one responsibility. Main .ino stays thin — just wiring.
