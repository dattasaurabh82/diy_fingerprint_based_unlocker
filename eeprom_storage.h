// ============================================================
// eeprom_storage.h — EEPROM read/write/verify for password + slot
// Header-only module
//
// Layout (36 bytes):
//   0x00: Magic (0xA5)
//   0x01: Active slot (1 or 2)
//   0x02: Password length (1-32)
//   0x03-0x22: Password (32 bytes, null-padded)
//   0x23: Checksum (XOR of bytes 0x00-0x22)
// ============================================================
#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ─── Init ───
inline void eepromInit() {
  EEPROM.begin(EEPROM_SIZE);
}

// ─── Checksum ───
static inline uint8_t _eepromCalcChecksum() {
  uint8_t cs = 0;
  for (uint16_t i = EEPROM_ADDR_MAGIC; i < EEPROM_ADDR_CHECKSUM; i++) {
    cs ^= EEPROM.read(i);
  }
  return cs;
}

// ─── Read registration ───
// Returns true if valid registration exists.
// Fills activeSlot, password buffer, and length.
inline bool eepromReadRegistration(uint8_t &activeSlot, char* password, uint8_t &length) {
  // Check magic
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_VALUE) return false;

  // Read active slot
  activeSlot = EEPROM.read(EEPROM_ADDR_ACTIVE_SLOT);
  if (activeSlot != 1 && activeSlot != 2) return false;

  // Read password length
  length = EEPROM.read(EEPROM_ADDR_PWD_LEN);
  if (length == 0 || length > PASSWORD_MAX_LEN) return false;

  // Read password bytes
  for (uint8_t i = 0; i < PASSWORD_MAX_LEN; i++) {
    password[i] = (char)EEPROM.read(EEPROM_ADDR_PWD_START + i);
  }
  password[length] = '\0';  // null terminate

  // Verify checksum
  uint8_t stored = EEPROM.read(EEPROM_ADDR_CHECKSUM);
  uint8_t calc = _eepromCalcChecksum();
  return (stored == calc);
}

// ─── Write registration ───
// Writes and commits. Returns true if verify passes after write.
inline bool eepromWriteRegistration(uint8_t activeSlot, const char* password, uint8_t length) {
  // Validate inputs
  if ((activeSlot != 1 && activeSlot != 2) || length == 0 || length > PASSWORD_MAX_LEN) {
    return false;
  }

  // Write magic
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VALUE);

  // Write active slot
  EEPROM.write(EEPROM_ADDR_ACTIVE_SLOT, activeSlot);

  // Write password length
  EEPROM.write(EEPROM_ADDR_PWD_LEN, length);

  // Write password bytes (null-pad remainder)
  for (uint8_t i = 0; i < PASSWORD_MAX_LEN; i++) {
    EEPROM.write(EEPROM_ADDR_PWD_START + i, (i < length) ? password[i] : 0);
  }

  // Calculate and write checksum
  uint8_t cs = _eepromCalcChecksum();
  EEPROM.write(EEPROM_ADDR_CHECKSUM, cs);

  // Commit to flash
  EEPROM.commit();

  // Verify by re-reading
  uint8_t slotBack;
  char pwdBack[PASSWORD_MAX_LEN + 1];
  uint8_t lenBack;
  if (!eepromReadRegistration(slotBack, pwdBack, lenBack)) return false;
  if (slotBack != activeSlot || lenBack != length) return false;
  if (memcmp(pwdBack, password, length) != 0) return false;

  return true;
}

// ─── Clear registration ───
inline void eepromClearRegistration() {
  EEPROM.write(EEPROM_ADDR_MAGIC, 0x00);
  EEPROM.commit();
}

// ─── Convenience: get active slot (0 = none/virgin) ───
inline uint8_t eepromGetActiveSlot() {
  uint8_t slot;
  char pwd[PASSWORD_MAX_LEN + 1];
  uint8_t len;
  if (eepromReadRegistration(slot, pwd, len)) return slot;
  return 0;
}

// ─── Convenience: get staging slot ───
inline uint8_t eepromGetStagingSlot() {
  uint8_t active = eepromGetActiveSlot();
  if (active == 1) return 2;
  if (active == 2) return 1;
  return 1;  // virgin: first registration goes to slot 1
}

// ─── Convenience: check if valid registration exists ───
inline bool eepromHasRegistration() {
  return (eepromGetActiveSlot() != 0);
}

#endif // EEPROM_STORAGE_H
