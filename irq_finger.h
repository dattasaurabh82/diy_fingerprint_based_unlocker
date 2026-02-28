// ============================================================
// irq_finger.h — IRQ-based finger detection via SEN0348 Touch Out
// Header-only module
//
// The SEN0348's blue IRQ wire (Touch Out) goes HIGH when a finger
// touches the sensor. This replaces the polling approach
// (detectFinger() every 50ms) with an interrupt-driven edge detect.
//
// Usage:
//   irqFingerInit()       — call once in setup after sensor init
//   irqFingerDetected()   — returns true once per touch (auto-clears)
//   irqFingerClear()      — manually clear flag (e.g., on mode switch)
//
// Note: detectFinger() is still used for finger-removal waits
// inside registration/recognition flows. IRQ only replaces the
// "is a new finger present?" polling in the main loop.
// ============================================================
#ifndef IRQ_FINGER_H
#define IRQ_FINGER_H

#include <Arduino.h>
#include "config.h"

// ─── Volatile flag set by ISR ───
static volatile bool _irq_fingerTouchFlag = false;

// ─── ISR — keep minimal (no Serial, no delays) ───
static void _irqOnFingerTouch() {
  _irq_fingerTouchFlag = true;
}

// ─── Init: attach interrupt on sensor's Touch Out pin ───
// Call after sensor is initialized and confirmed working.
inline void irqFingerInit() {
  pinMode(PIN_IRQ, INPUT_PULLDOWN);  // Touch Out is active-HIGH
  attachInterrupt(digitalPinToInterrupt(PIN_IRQ), _irqOnFingerTouch, RISING);
  Serial.println("[BOOT] IRQ finger detection OK (GPIO" + String(PIN_IRQ) + ")");
}

// ─── Check if a new finger touch was detected ───
// Returns true exactly once per touch event (auto-clears the flag).
inline bool irqFingerDetected() {
  if (_irq_fingerTouchFlag) {
    _irq_fingerTouchFlag = false;
    return true;
  }
  return false;
}

// ─── Manually clear the flag ───
// Call on mode switch or after handling a touch to avoid stale triggers.
inline void irqFingerClear() {
  _irq_fingerTouchFlag = false;
}

#endif // IRQ_FINGER_H
