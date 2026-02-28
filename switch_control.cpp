// ============================================================
// switch_control.cpp — SPDT switch with software debounce
// ============================================================

#include "switch_control.h"
#include "config.h"

static int _lastReading    = -1;
static unsigned long _lastChangeTime = 0;
static DeviceMode _stableMode = MODE_RECOGNIZE;  // default: HIGH (pull-up)
static bool _initialized = false;
static bool _changed = false;

void switchInit() {
  pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);

  // Read initial state immediately (no debounce needed at boot)
  int raw = digitalRead(PIN_MODE_SWITCH);
  _stableMode = (raw == LOW) ? MODE_REGISTER : MODE_RECOGNIZE;
  _lastReading = raw;
  _lastChangeTime = millis();
  _initialized = true;
  _changed = false;
}

DeviceMode switchRead() {
  if (!_initialized) {
    switchInit();
  }

  int reading = digitalRead(PIN_MODE_SWITCH);

  // Reset debounce timer on any change
  if (reading != _lastReading) {
    _lastChangeTime = millis();
    _lastReading = reading;
  }

  // Only accept new state after debounce period
  if ((millis() - _lastChangeTime) > DEBOUNCE_MS) {
    DeviceMode newMode = (reading == LOW) ? MODE_REGISTER : MODE_RECOGNIZE;
    if (newMode != _stableMode) {
      _stableMode = newMode;
      _changed = true;
    }
  }

  return _stableMode;
}

bool switchChanged() {
  return _changed;
}

void switchAckChange() {
  _changed = false;
}

const char* modeName(DeviceMode mode) {
  return (mode == MODE_REGISTER) ? "REGISTER" : "RECOGNIZE";
}
