// ============================================================
// hid_unlock.h — Mac HID unlock sequence
//
// Proven sequence (Test 8):
//   1. Ctrl+Cmd+Q  (lock screen — safe if already locked)
//   2. LEFT_CTRL x2 (wake — non-printable, won't type in pwd field)
//   3. Cmd+A (select-all — clears stale text in pwd field)
//   4. Type password (first char replaces selection from step 3)
//   5. Enter (submit)
// ============================================================
#ifndef HID_UNLOCK_H
#define HID_UNLOCK_H

#include <Arduino.h>
#include <Keyboard.h>
#include "config.h"

// ─── Init ───
inline void hidInit() {
  Keyboard.begin();
}

// ─── End ───
inline void hidEnd() {
  Keyboard.end();
}

// ─── Execute full Mac unlock sequence ───
// password: null-terminated string to type
// skipLock: if true, skip step 1 (Ctrl+Cmd+Q) — for testing only
inline void hidUnlockSequence(const char* password, bool skipLock = false) {

  // Step 1: Lock screen (Ctrl+Cmd+Q)
  if (!skipLock) {
    Serial.println("[HID] Lock (Ctrl+Cmd+Q)");
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('q');
    delay(50);
    Keyboard.releaseAll();
    delay(LOCK_DELAY_MS);
  }

  // Step 2: Wake display (LEFT_CTRL x N — non-printable)
  Serial.println("[HID] Wake (LEFT_CTRL x2)");
  for (uint8_t i = 0; i < WAKE_PRESSES; i++) {
    Keyboard.press(KEY_LEFT_CTRL);
    delay(50);
    Keyboard.release(KEY_LEFT_CTRL);
    delay(WAKE_PRESS_DELAY_MS);
  }
  delay(WAKE_SETTLE_MS);

  // Step 3: Clear password field (Cmd+A → select all)
  Serial.println("[HID] Clear field (Cmd+A)");
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('a');
  delay(50);
  Keyboard.releaseAll();
  delay(FIELD_CLEAR_DELAY_MS);

  // Step 4: Type password
  Serial.println("[HID] Typing password...");
  Keyboard.print(password);
  delay(POST_TYPE_DELAY_MS);

  // Step 5: Press Enter
  Serial.println("[HID] Enter");
  Keyboard.press(KEY_RETURN);
  delay(50);
  Keyboard.release(KEY_RETURN);
  delay(POST_ENTER_DELAY_MS);

  Serial.println("[HID] Unlock sequence complete");
}

#endif // HID_UNLOCK_H
