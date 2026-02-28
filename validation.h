// ============================================================
// validation.h — Boot integrity check + orphan cleanup
// Header-only module
//
// Decision matrix (from PLAN.md):
//   EEPROM valid + activeSlot fingerprint exists   → VALID (clean orphans)
//   EEPROM invalid + no fingerprints               → VIRGIN
//   EEPROM valid + activeSlot fingerprint missing   → CORRUPT (clear all)
//   EEPROM invalid + fingerprint(s) exist           → CORRUPT (clear all)
//
// Slot check: uses getEnrolledIDList() to build a lookup of occupied slots.
// ============================================================
#ifndef VALIDATION_H
#define VALIDATION_H

#include <Arduino.h>
#include <DFRobot_ID809.h>
#include "config.h"
#include "eeprom_storage.h"
#include "led_feedback.h"

// ─── Result codes ───
enum BootState {
  BOOT_VALID,    // Good registration, ready to use
  BOOT_VIRGIN,   // No registration, needs first enrollment
  BOOT_CORRUPT   // Inconsistent state, was cleaned up
};

// ─── Slot occupancy state (built once at validation time) ───
static bool _val_slot1Occupied = false;
static bool _val_slot2Occupied = false;

// ─── Build slot occupancy map using getEnrolledIDList ───
// More reliable than getStatusID which may have unexpected return values.
static inline void _valBuildSlotMap(DFRobot_ID809 &fp) {
  _val_slot1Occupied = false;
  _val_slot2Occupied = false;

  uint8_t count = fp.getEnrollCount();
  if (count == 0) return;

  // Allocate buffer for enrolled ID list (sensor supports up to 80)
  uint8_t idList[80];
  memset(idList, 0, sizeof(idList));

  uint8_t ret = fp.getEnrolledIDList(idList);
  if (ret != 0) {
    // getEnrolledIDList failed — fall back to count-only check
    Serial.println("[BOOT] Warning: getEnrolledIDList failed, using count only");
    // If count > 0 but we can't get the list, assume worst case
    // Mark both as potentially occupied to avoid false VIRGIN
    _val_slot1Occupied = true;
    _val_slot2Occupied = (count > 1);
    return;
  }

  // Scan the list for slots 1 and 2
  for (uint8_t i = 0; i < count && i < 80; i++) {
    if (idList[i] == 1) _val_slot1Occupied = true;
    if (idList[i] == 2) _val_slot2Occupied = true;
  }
}

// ─── Delete all fingerprints in our two slots ───
static inline void _valClearAllSlots(DFRobot_ID809 &fp) {
  fp.delFingerprint(1);
  fp.delFingerprint(2);
}

// ─── Main boot validation ───
// Call after sensor + EEPROM are initialized, before entering main loop.
// Returns the boot state so the caller can decide behavior.
inline BootState runBootValidation(DFRobot_ID809 &fp) {
  Serial.println("[BOOT] Running integrity check...");
  Serial.flush();

  // Read EEPROM state
  uint8_t activeSlot = 0;
  char password[PASSWORD_MAX_LEN + 1];
  uint8_t pwdLen = 0;
  bool eepromValid = eepromReadRegistration(activeSlot, password, pwdLen);

  // Clear password from stack immediately — we don't need it for validation
  memset(password, 0, sizeof(password));

  // Build slot occupancy map from sensor
  _valBuildSlotMap(fp);

  bool anyFingerprints = _val_slot1Occupied || _val_slot2Occupied;

  // Detailed debug output
  Serial.print("[BOOT] EEPROM: ");
  if (eepromValid) {
    Serial.println("valid (slot " + String(activeSlot) + ")");
  } else {
    Serial.println("invalid");
  }
  Serial.flush();

  Serial.print("[BOOT] Sensor: slot1=");
  Serial.print(_val_slot1Occupied ? "occupied" : "empty");
  Serial.print(" slot2=");
  Serial.println(_val_slot2Occupied ? "occupied" : "empty");
  Serial.flush();

  // ── Case 1: EEPROM valid + active slot has fingerprint → VALID ──
  bool activeSlotOccupied = (activeSlot == 1) ? _val_slot1Occupied : _val_slot2Occupied;

  if (eepromValid && activeSlotOccupied) {
    Serial.println("[BOOT] State: VALID");
    Serial.flush();

    // Clean up orphan in the OTHER slot (from interrupted registration)
    uint8_t otherSlot = (activeSlot == 1) ? 2 : 1;
    bool otherOccupied = (otherSlot == 1) ? _val_slot1Occupied : _val_slot2Occupied;
    if (otherOccupied) {
      fp.delFingerprint(otherSlot);
      Serial.println("[BOOT] Cleaned orphan in slot " + String(otherSlot));
    }

    return BOOT_VALID;
  }

  // ── Case 2: EEPROM invalid + no fingerprints → VIRGIN ──
  if (!eepromValid && !anyFingerprints) {
    Serial.println("[BOOT] State: VIRGIN");
    Serial.flush();
    return BOOT_VIRGIN;
  }

  // ── Case 3: EEPROM valid + active slot fingerprint MISSING → CORRUPT ──
  if (eepromValid && !activeSlotOccupied) {
    Serial.println("[WARNING] Fingerprint missing for active slot — corrupt");
    Serial.flush();
    ledCorruptState();

    eepromClearRegistration();
    _valClearAllSlots(fp);

    Serial.println("[WARNING] Cleared EEPROM + all fingerprints");
    Serial.flush();
    delay(2000);
    return BOOT_CORRUPT;
  }

  // ── Case 4: EEPROM invalid + fingerprint(s) exist → CORRUPT ──
  if (!eepromValid && anyFingerprints) {
    Serial.println("[WARNING] Orphan fingerprint(s) without password — corrupt");
    Serial.flush();
    ledCorruptState();

    _valClearAllSlots(fp);

    Serial.println("[WARNING] Cleared orphan fingerprints");
    Serial.flush();
    delay(2000);
    return BOOT_CORRUPT;
  }

  // Shouldn't reach here, but treat as virgin
  Serial.println("[WARNING] Unexpected state — treating as virgin");
  Serial.flush();
  return BOOT_VIRGIN;
}

#endif // VALIDATION_H
