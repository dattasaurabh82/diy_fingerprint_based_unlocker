// ============================================================
// crypto.h — Device-bound AES-256-CBC encryption for EEPROM
//
// Key derivation:
//   1. Read RP2350's 8-byte unique board ID from OTP
//   2. SHA-256 hash it → 32-byte AES key
//   3. SHA-256 hash (unique_id + salt) → first 16 bytes = IV
//
// The encryption key is DEVICE-SPECIFIC. An EEPROM dump from
// one board is useless on another (or without the board).
//
// Usage:
//   cryptoInit()                           — call once at boot
//   cryptoEncryptPassword(plain, cipher)   — encrypt 32 bytes
//   cryptoDecryptPassword(cipher, plain)   — decrypt 32 bytes
// ============================================================
#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include <pico/unique_id.h>
#include "tiny_aes.h"

// ─── Minimal software SHA-256 (RFC 6234) ───
// Used for key derivation only. ~100 lines.

static const uint32_t _sha256_k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t _sha_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t _sha_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t _sha_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t _sha_ep0(uint32_t x) { return _sha_rotr(x,2) ^ _sha_rotr(x,13) ^ _sha_rotr(x,22); }
static inline uint32_t _sha_ep1(uint32_t x) { return _sha_rotr(x,6) ^ _sha_rotr(x,11) ^ _sha_rotr(x,25); }
static inline uint32_t _sha_sig0(uint32_t x) { return _sha_rotr(x,7) ^ _sha_rotr(x,18) ^ (x >> 3); }
static inline uint32_t _sha_sig1(uint32_t x) { return _sha_rotr(x,17) ^ _sha_rotr(x,19) ^ (x >> 10); }

// SHA-256: hash arbitrary data into 32-byte digest
// Simple single-call implementation for short inputs (< 55 bytes)
static inline void _sha256(const uint8_t* data, size_t len, uint8_t digest[32]) {
  uint32_t h[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };

  // Pad message: data + 0x80 + zeros + 64-bit length
  // For inputs <= 55 bytes, fits in single 64-byte block
  uint8_t block[64];
  memset(block, 0, 64);
  memcpy(block, data, len);
  block[len] = 0x80;

  // Length in bits (big-endian) at end of block
  uint64_t bitlen = len * 8;
  block[63] = (uint8_t)(bitlen);
  block[62] = (uint8_t)(bitlen >> 8);
  block[61] = (uint8_t)(bitlen >> 16);
  block[60] = (uint8_t)(bitlen >> 24);
  block[59] = (uint8_t)(bitlen >> 32);
  block[58] = (uint8_t)(bitlen >> 40);
  block[57] = (uint8_t)(bitlen >> 48);
  block[56] = (uint8_t)(bitlen >> 56);

  // Parse block into 16 big-endian words
  uint32_t w[64];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
            ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
  }
  for (int i = 16; i < 64; i++) {
    w[i] = _sha_sig1(w[i-2]) + w[i-7] + _sha_sig0(w[i-15]) + w[i-16];
  }

  // Compress
  uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = hh + _sha_ep1(e) + _sha_ch(e,f,g) + _sha256_k[i] + w[i];
    uint32_t t2 = _sha_ep0(a) + _sha_maj(a,b,c);
    hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
  }
  h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;

  // Output digest (big-endian)
  for (int i = 0; i < 8; i++) {
    digest[i*4+0] = (uint8_t)(h[i] >> 24);
    digest[i*4+1] = (uint8_t)(h[i] >> 16);
    digest[i*4+2] = (uint8_t)(h[i] >> 8);
    digest[i*4+3] = (uint8_t)(h[i]);
  }
}

// ============================================================
// Device key material — derived once at boot, held in RAM
// ============================================================

static uint8_t _crypto_key[32];  // AES-256 key (SHA-256 of unique ID)
static uint8_t _crypto_iv[16];   // CBC IV (from salted hash of unique ID)
static bool _crypto_ready = false;

// ─── Init: derive device-specific key + IV from unique board ID ───
inline void cryptoInit() {
  // 1. Read unique board ID (8 bytes from OTP)
  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);

  // 2. Derive AES key: SHA-256(unique_id)
  _sha256(board_id.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES, _crypto_key);

  // 3. Derive IV: SHA-256(unique_id + salt) → take first 16 bytes
  //    Salt ensures IV differs from key even though same source
  uint8_t salted[PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 4];
  memcpy(salted, board_id.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);
  salted[PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 0] = 0xDE;
  salted[PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1] = 0xAD;
  salted[PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 2] = 0xBE;
  salted[PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 3] = 0xEF;

  uint8_t ivHash[32];
  _sha256(salted, sizeof(salted), ivHash);
  memcpy(_crypto_iv, ivHash, 16);

  // Clear intermediates
  memset(&board_id, 0, sizeof(board_id));
  memset(salted, 0, sizeof(salted));
  memset(ivHash, 0, sizeof(ivHash));

  _crypto_ready = true;

  Serial.println("[BOOT] Crypto OK (AES-256-CBC, device-bound key)");
}

// ─── Encrypt 32-byte password buffer ───
// plaintext: input (32 bytes, null-padded)
// ciphertext: output (32 bytes)
// Both can be the same buffer (in-place).
inline bool cryptoEncryptPassword(const uint8_t* plaintext, uint8_t* ciphertext) {
  if (!_crypto_ready) return false;

  // Copy plaintext to output (CBC encrypts in-place)
  if (plaintext != ciphertext) {
    memcpy(ciphertext, plaintext, 32);
  }

  // Fresh context each time (IV must be reset for deterministic encryption)
  AesCtx ctx;
  aesInitCtx(&ctx, _crypto_key, _crypto_iv);
  aesCbcEncrypt(&ctx, ciphertext, 32);

  // Clear context from stack
  memset(&ctx, 0, sizeof(ctx));
  return true;
}

// ─── Decrypt 32-byte password buffer ───
// ciphertext: input (32 bytes)
// plaintext: output (32 bytes)
// Both can be the same buffer (in-place).
inline bool cryptoDecryptPassword(const uint8_t* ciphertext, uint8_t* plaintext) {
  if (!_crypto_ready) return false;

  if (ciphertext != plaintext) {
    memcpy(plaintext, ciphertext, 32);
  }

  AesCtx ctx;
  aesInitCtx(&ctx, _crypto_key, _crypto_iv);
  aesCbcDecrypt(&ctx, plaintext, 32);

  memset(&ctx, 0, sizeof(ctx));
  return true;
}

#endif // CRYPTO_H
