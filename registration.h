// ============================================================
// registration.h — Two-slot safe fingerprint + password registration
//
// Key principle: never destroy old registration until new one is
// fully committed and verified. Old pair stays intact on any failure.
// ============================================================
#ifndef REGISTRATION_H
#define REGISTRATION_H

#include <Arduino.h>
#include <DFRobot_ID809.h>
#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"
#include "eeprom_storage.h"

// ─── State for abort detection ───
static uint8_t _reg_stagingSlot = 0;
static bool _reg_fingerprintStored = false;

// ─── Abort check: returns true if switch changed mid-operation ───
static inline bool _regCheckAbort() {
  switchRead();
  if (switchChanged()) {
    Serial.println("[WARNING] Switch changed — aborting registration");
    return true;
  }
  return false;
}

// ─── Rollback: clean up staging slot, preserve old registration ───
static inline void _regRollback(DFRobot_ID809 &fp) {
  if (_reg_fingerprintStored && _reg_stagingSlot > 0) {
    fp.delFingerprint(_reg_stagingSlot);
    Serial.print("[REG] Cleaned staging slot ");
    Serial.println(_reg_stagingSlot);
  }
  _reg_fingerprintStored = false;
  Serial.println("[REG] Rolled back — old registration preserved");
}

// ─── Read password from Serial with masked echo ───
// Returns length of password read, 0 on timeout or abort.
static inline uint8_t _regReadPassword(char* buf, const char* prompt) {
  Serial.println(prompt);
  memset(buf, 0, PASSWORD_MAX_LEN + 1);

  uint8_t idx = 0;
  unsigned long startTime = millis();

  while (idx < PASSWORD_MAX_LEN) {
    // Check abort
    if (_regCheckAbort()) return 0;

    // Check timeout
    if ((millis() - startTime) > PASSWORD_TIMEOUT_MS) {
      Serial.println();
      Serial.println("[REG] Password entry timeout");
      return 0;
    }

    if (Serial.available()) {
      char c = Serial.read();

      if (c == '\n' || c == '\r') {
        Serial.println();  // newline after masked input
        if (idx == 0) {
          Serial.println("[REG] Empty password not allowed");
          Serial.println(prompt);
          startTime = millis();  // reset timeout
          continue;
        }
        buf[idx] = '\0';
        return idx;
      } else if (c == '\b' || c == 127) {
        // Backspace
        if (idx > 0) {
          idx--;
          Serial.print("\b \b");  // erase last '*'
        }
      } else if (c >= 32 && c <= 126) {
        // Printable char
        buf[idx++] = c;
        Serial.print('*');
        startTime = millis();  // reset timeout on activity
      }
    }

    delay(10);
  }

  // Buffer full
  buf[idx] = '\0';
  Serial.println();
  return idx;
}

// ─── Main registration flow ───
// Returns true if registration succeeded.
// fingerprint: reference to the DFRobot_ID809 instance
inline bool runRegistration(DFRobot_ID809 &fp) {
  Serial.println("[MODE] REGISTER");

  // Reset state
  _reg_fingerprintStored = false;

  // ── Determine slots ──
  uint8_t activeSlot = eepromGetActiveSlot();
  _reg_stagingSlot = eepromGetStagingSlot();

  Serial.print("[REG] Active slot: ");
  Serial.print(activeSlot == 0 ? "none (virgin)" : String(activeSlot).c_str());
  Serial.print(", staging to slot: ");
  Serial.println(_reg_stagingSlot);

  // ── Step 1: Clean staging slot ──
  fp.delFingerprint(_reg_stagingSlot);  // ignore error if empty
  Serial.print("[REG] Cleaned staging slot ");
  Serial.println(_reg_stagingSlot);

  // ── Step 2: Fingerprint enrollment (3× capture to staging slot) ──
  ledWaitingFinger();

  for (uint8_t i = 0; i < COLLECT_COUNT; i++) {
    uint8_t retries = 0;

    while (retries < MAX_CAPTURE_RETRIES) {
      // Check abort before each capture
      if (_regCheckAbort()) {
        _regRollback(fp);
        return false;
      }

      Serial.print("[REG] Place finger (");
      Serial.print(i + 1);
      Serial.print("/");
      Serial.print(COLLECT_COUNT);
      Serial.println(")...");
      ledWaitingFinger();

      // Wait for finger with timeout
      uint8_t ret = fp.collectionFingerprint(CAPTURE_TIMEOUT);

      if (ret != ERR_ID809) {
        // Capture succeeded
        ledCaptureOK();
        Serial.print("[REG] Captured ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.println(COLLECT_COUNT);

        // Wait for finger removal
        Serial.println("[REG] Remove finger...");
        while (fp.detectFinger()) {
          if (_regCheckAbort()) {
            _regRollback(fp);
            return false;
          }
          delay(100);
        }
        delay(500);
        break;  // move to next capture
      } else {
        // Capture failed
        ledCaptureFail();
        retries++;
        Serial.print("[REG] Capture failed (attempt ");
        Serial.print(retries);
        Serial.print("/");
        Serial.print(MAX_CAPTURE_RETRIES);
        Serial.println(")");

        if (retries >= MAX_CAPTURE_RETRIES) {
          Serial.println("[REG] Max retries — enrollment failed");
          ledRegisterFail();
          _regRollback(fp);
          return false;
        }

        delay(1000);

        // Wait for finger removal before retry
        while (fp.detectFinger()) delay(100);
      }
    }
  }

  // Store fingerprint to staging slot
  Serial.print("[REG] Storing to staging slot ");
  Serial.print(_reg_stagingSlot);
  Serial.print("... ");

  uint8_t storeResult = fp.storeFingerprint(_reg_stagingSlot);
  if (storeResult != 0) {
    Serial.println("FAILED");
    ledRegisterFail();
    _regRollback(fp);
    return false;
  }

  Serial.println("OK");
  _reg_fingerprintStored = true;

  // ── Step 3: Password input via Serial ──
  ledWaitingPassword();

  char password[PASSWORD_MAX_LEN + 1];
  char confirm[PASSWORD_MAX_LEN + 1];

  uint8_t pwdLen = _regReadPassword(password, "[REG] Enter password (max 32 chars, Enter to confirm):");
  if (pwdLen == 0) {
    ledRegisterFail();
    _regRollback(fp);
    return false;
  }

  // Confirm password with retries
  for (uint8_t attempt = 0; attempt < PASSWORD_MAX_CONFIRM_ATTEMPTS; attempt++) {
    uint8_t confirmLen = _regReadPassword(confirm, "[REG] Confirm password:");
    if (confirmLen == 0) {
      ledRegisterFail();
      _regRollback(fp);
      return false;
    }

    if (pwdLen == confirmLen && memcmp(password, confirm, pwdLen) == 0) {
      // Match — proceed to commit
      goto password_confirmed;
    }

    // Mismatch
    Serial.print("[REG] Mismatch! (attempt ");
    Serial.print(attempt + 1);
    Serial.print("/");
    Serial.print(PASSWORD_MAX_CONFIRM_ATTEMPTS);
    Serial.println(")");

    if (attempt + 1 >= PASSWORD_MAX_CONFIRM_ATTEMPTS) {
      Serial.println("[REG] Too many mismatches");
      ledRegisterFail();
      _regRollback(fp);
      return false;
    }
  }

password_confirmed:
  // Clear confirm buffer
  memset(confirm, 0, sizeof(confirm));

  // ── Step 4: Atomic commit ──
  Serial.println("[REG] Committing...");

  // Backup old registration in case we need to restore
  uint8_t oldSlot = 0;
  char oldPwd[PASSWORD_MAX_LEN + 1] = {0};
  uint8_t oldLen = 0;
  bool hadOldReg = eepromReadRegistration(oldSlot, oldPwd, oldLen);

  // Write new registration
  if (!eepromWriteRegistration(_reg_stagingSlot, password, pwdLen)) {
    Serial.println("[REG] EEPROM verify failed!");
    ledRegisterFail();

    // Restore old EEPROM if there was a previous registration
    if (hadOldReg) {
      eepromWriteRegistration(oldSlot, oldPwd, oldLen);
      Serial.println("[REG] Old EEPROM data restored");
    } else {
      eepromClearRegistration();
    }

    // Clean staging fingerprint
    _regRollback(fp);
    return false;
  }

  // ── Success! Now safe to delete old slot ──
  if (activeSlot > 0 && activeSlot != _reg_stagingSlot) {
    fp.delFingerprint(activeSlot);
    Serial.print("[REG] Deleted old slot ");
    Serial.println(activeSlot);
  }

  // Clear sensitive data from RAM
  memset(password, 0, sizeof(password));
  memset(oldPwd, 0, sizeof(oldPwd));

  ledRegisterSuccess();
  Serial.print("[REG] Registration complete (slot ");
  Serial.print(_reg_stagingSlot);
  Serial.println(" now active)");

  delay(2000);  // show green LED
  _reg_fingerprintStored = false;

  return true;
}

#endif // REGISTRATION_H
