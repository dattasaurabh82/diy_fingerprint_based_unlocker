// ============================================================
// eeprom_storage.h — Encrypted EEPROM storage for password + slot
// Header-only module
//
// Layout (36 bytes):
//   0x00: Magic (0xAE = encrypted format)
//   0x01: Active slot (1 or 2)
//   0x02: Password length (1-32, stored in plaintext)
//   0x03-0x22: ENCRYPTED password (32 bytes AES-256-CBC)
//   0x23: Checksum (XOR of bytes 0x00-0x22, over encrypted data)
//
// The password is encrypted with a device-specific AES-256 key
// derived from the RP2350's unique board ID. An EEPROM dump
// from one board cannot be decrypted on another.
// ============================================================
#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "crypto.h"

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
// Fills activeSlot, password buffer (decrypted), and length.
inline bool eepromReadRegistration(uint8_t &activeSlot, char* password, uint8_t &length) {
  // Check magic
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC_VALUE) return false;

  // Read active slot
  activeSlot = EEPROM.read(EEPROM_ADDR_ACTIVE_SLOT);
  if (activeSlot != 1 && activeSlot != 2) return false;

  // Read password length (stored in plaintext)
  length = EEPROM.read(EEPROM_ADDR_PWD_LEN);
  if (length == 0 || length > PASSWORD_MAX_LEN) return false;

  // Verify checksum BEFORE decryption (checksum covers encrypted data)
  uint8_t stored = EEPROM.read(EEPROM_ADDR_CHECKSUM);
  uint8_t calc = _eepromCalcChecksum();
  if (stored != calc) return false;

  // Read encrypted password bytes
  uint8_t encrypted[PASSWORD_MAX_LEN];
  for (uint8_t i = 0; i < PASSWORD_MAX_LEN; i++) {
    encrypted[i] = EEPROM.read(EEPROM_ADDR_PWD_START + i);
  }

  // Decrypt
  uint8_t decrypted[PASSWORD_MAX_LEN];
  if (!cryptoDecryptPassword(encrypted, decrypted)) return false;

  // Copy to output
  memcpy(password, decrypted, length);
  password[length] = '\0';

  // Clear temp buffers
  memset(encrypted, 0, sizeof(encrypted));
  memset(decrypted, 0, sizeof(decrypted));

  return true;
}

// ─── Write registration ───
// Encrypts password, writes to EEPROM, commits. Returns true if verify passes.
inline bool eepromWriteRegistration(uint8_t activeSlot, const char* password, uint8_t length) {
  // Validate inputs
  if ((activeSlot != 1 && activeSlot != 2) || length == 0 || length > PASSWORD_MAX_LEN) {
    return false;
  }

  // Prepare plaintext buffer (null-padded to 32 bytes)
  uint8_t plaintext[PASSWORD_MAX_LEN];
  memset(plaintext, 0, PASSWORD_MAX_LEN);
  memcpy(plaintext, password, length);

  // Encrypt
  uint8_t ciphertext[PASSWORD_MAX_LEN];
  if (!cryptoEncryptPassword(plaintext, ciphertext)) {
    memset(plaintext, 0, sizeof(plaintext));
    return false;
  }
  memset(plaintext, 0, sizeof(plaintext));

  // Write magic
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC_VALUE);

  // Write active slot
  EEPROM.write(EEPROM_ADDR_ACTIVE_SLOT, activeSlot);

  // Write password length (plaintext — needed for decryption output sizing)
  EEPROM.write(EEPROM_ADDR_PWD_LEN, length);

  // Write encrypted password bytes
  for (uint8_t i = 0; i < PASSWORD_MAX_LEN; i++) {
    EEPROM.write(EEPROM_ADDR_PWD_START + i, ciphertext[i]);
  }

  // Calculate and write checksum (over encrypted data)
  uint8_t cs = _eepromCalcChecksum();
  EEPROM.write(EEPROM_ADDR_CHECKSUM, cs);

  // Commit to flash
  EEPROM.commit();

  // Verify by re-reading (which decrypts)
  uint8_t slotBack;
  char pwdBack[PASSWORD_MAX_LEN + 1];
  uint8_t lenBack;
  if (!eepromReadRegistration(slotBack, pwdBack, lenBack)) return false;
  if (slotBack != activeSlot || lenBack != length) return false;
  if (memcmp(pwdBack, password, length) != 0) return false;

  // Clear sensitive data
  memset(ciphertext, 0, sizeof(ciphertext));
  memset(pwdBack, 0, sizeof(pwdBack));

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
