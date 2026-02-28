// ============================================================
// recognition.h — Fingerprint match + HID unlock flow
// Header-only module
//
// Flow:
//   1. Finger detected → capture → search
//   2. Match → read password from EEPROM → HID unlock sequence
//   3. No match → red LED, continue waiting
//   4. 5s cooldown between successful unlocks
// ============================================================
#ifndef RECOGNITION_H
#define RECOGNITION_H

#include <Arduino.h>
#include <DFRobot_ID809.h>
#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"
#include "eeprom_storage.h"
#include "hid_unlock.h"

// ─── State ───
static unsigned long _rec_cooldownUntil = 0;
static bool _rec_noRegistration = false;

// ─── Check if in cooldown ───
static inline bool _recInCooldown() {
  if (_rec_cooldownUntil == 0) return false;
  if (millis() < _rec_cooldownUntil) return true;
  // Cooldown expired
  _rec_cooldownUntil = 0;
  return false;
}

// ─── Validate registration exists (call once on mode entry) ───
// Returns true if a valid registration exists (fingerprint + password).
inline bool recCheckRegistration(DFRobot_ID809 &fp) {
  uint8_t activeSlot = eepromGetActiveSlot();
  if (activeSlot == 0) {
    _rec_noRegistration = true;
    return false;
  }

  // Verify the sensor actually has a fingerprint in that slot
  // We do this by checking enrolled count — if 0, no fingerprints at all
  if (fp.getEnrollCount() == 0) {
    _rec_noRegistration = true;
    return false;
  }

  _rec_noRegistration = false;
  return true;
}

// ─── Handle a single recognition cycle ───
// Called from main loop when finger is newly detected in RECOGNIZE mode.
// Returns true if unlock sequence was sent.
inline bool runRecognition(DFRobot_ID809 &fp) {
  // Guard: no registration
  if (_rec_noRegistration) {
    Serial.println("[AUTH] No registration — flip to REGISTER");
    ledNoRegistration();
    return false;
  }

  // Guard: cooldown active
  if (_recInCooldown()) {
    Serial.println("[AUTH] Cooldown active — ignoring touch");
    return false;
  }

  // ── Capture fingerprint ──
  Serial.println("[AUTH] Capturing...");

  uint8_t ret = fp.collectionFingerprint(MATCH_TIMEOUT);
  if (ret == ERR_ID809) {
    Serial.println("[AUTH] Capture failed");
    ledCaptureFail();
    delay(1000);
    ledRecognizeReady();
    return false;
  }

  // ── Search for match ──
  uint8_t matchID = fp.search();

  if (matchID == 0 || matchID == ERR_ID809) {
    // No match
    Serial.println("[AUTH] No match");
    ledNoMatch();
    delay(1500);
    ledRecognizeReady();
    return false;
  }

  // ── Match found ──
  Serial.print("[AUTH] Match — slot #");
  Serial.println(matchID);

  // Verify this is our active slot
  uint8_t activeSlot = eepromGetActiveSlot();
  if (matchID != activeSlot) {
    // Matched an orphan slot, not our active registration
    Serial.print("[AUTH] Matched slot ");
    Serial.print(matchID);
    Serial.print(" but active is ");
    Serial.println(activeSlot);
    Serial.println("[AUTH] Ignoring orphan match");
    ledNoMatch();
    delay(1500);
    ledRecognizeReady();
    return false;
  }

  // ── Read password from EEPROM ──
  uint8_t slot;
  char password[PASSWORD_MAX_LEN + 1];
  uint8_t pwdLen;

  if (!eepromReadRegistration(slot, password, pwdLen)) {
    Serial.println("[AUTH] EEPROM read failed — registration corrupt?");
    ledNoRegistration();
    return false;
  }

  // ── Execute HID unlock ──
  ledMatchFound();
  Serial.println("[AUTH] Sending unlock sequence...");

  hidUnlockSequence(password);

  // Clear password from RAM immediately
  memset(password, 0, sizeof(password));

  Serial.println("[AUTH] Unlock complete");

  // ── Start cooldown ──
  _rec_cooldownUntil = millis() + COOLDOWN_MS;
  Serial.println("[AUTH] Cooldown 5s...");

  ledMatchFound();
  delay(2000);
  ledCooldown();

  return true;
}

// ─── Reset state (call on mode switch) ───
inline void recReset() {
  _rec_cooldownUntil = 0;
  _rec_noRegistration = false;
}

#endif // RECOGNITION_H
