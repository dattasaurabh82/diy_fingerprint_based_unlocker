// ============================================================
// switch_control.h — SPDT switch with software debounce
// Header-only module (implementation inline)
// ============================================================
#ifndef SWITCH_CONTROL_H
#define SWITCH_CONTROL_H

#include <Arduino.h>
#include "config.h"

// ─── Types ───
enum DeviceMode {
  MODE_REGISTER,   // Switch LOW (pulled to GND)
  MODE_RECOGNIZE   // Switch HIGH (internal pull-up)
};

// ─── State (file-scoped) ───
static int _sw_lastReading         = -1;
static unsigned long _sw_lastChangeTime = 0;
static DeviceMode _sw_stableMode   = MODE_RECOGNIZE;
static bool _sw_initialized        = false;
static bool _sw_changed            = false;

// ─── API ───

inline void switchInit() {
  pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);

  // Read initial state immediately (no debounce needed at boot)
  int raw = digitalRead(PIN_MODE_SWITCH);
  _sw_stableMode   = (raw == LOW) ? MODE_REGISTER : MODE_RECOGNIZE;
  _sw_lastReading  = raw;
  _sw_lastChangeTime = millis();
  _sw_initialized  = true;
  _sw_changed      = false;
}

inline DeviceMode switchRead() {
  if (!_sw_initialized) switchInit();

  int reading = digitalRead(PIN_MODE_SWITCH);

  // Reset debounce timer on any edge
  if (reading != _sw_lastReading) {
    _sw_lastChangeTime = millis();
    _sw_lastReading = reading;
  }

  // Accept new state only after debounce settles
  if ((millis() - _sw_lastChangeTime) > DEBOUNCE_MS) {
    DeviceMode newMode = (reading == LOW) ? MODE_REGISTER : MODE_RECOGNIZE;
    if (newMode != _sw_stableMode) {
      _sw_stableMode = newMode;
      _sw_changed = true;
    }
  }

  return _sw_stableMode;
}

inline bool switchChanged()    { return _sw_changed; }
inline void switchAckChange()  { _sw_changed = false; }

inline const char* modeName(DeviceMode mode) {
  return (mode == MODE_REGISTER) ? "REGISTER" : "RECOGNIZE";
}

#endif // SWITCH_CONTROL_H
