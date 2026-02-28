// ============================================================
// switch_control.h — SPDT switch with software debounce
// ============================================================
#ifndef SWITCH_CONTROL_H
#define SWITCH_CONTROL_H

#include <Arduino.h>

// Mode enum
enum DeviceMode {
  MODE_REGISTER,   // Switch LOW (pulled to GND)
  MODE_RECOGNIZE   // Switch HIGH (internal pull-up)
};

// Initialize switch GPIO
void switchInit();

// Read debounced switch state. Call frequently from loop().
// Returns current stable mode.
DeviceMode switchRead();

// Check if switch changed since last call to switchAckChange().
// Use this to detect mid-operation mode changes.
bool switchChanged();

// Acknowledge the switch change (reset the flag).
void switchAckChange();

// Get human-readable mode name
const char* modeName(DeviceMode mode);

#endif // SWITCH_CONTROL_H
