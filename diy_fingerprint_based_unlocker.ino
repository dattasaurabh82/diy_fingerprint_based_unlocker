// ============================================================
// DIY Fingerprint-Based Unlocker
// Milestone 1: Boot + Switch (debounced) + Sensor Init + LED
// ============================================================

#include <DFRobot_ID809.h>

#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"

// ─── Globals ───
DFRobot_ID809 fingerprint;
bool sensorOK = false;
DeviceMode currentMode = MODE_RECOGNIZE;

// ─── Forward declarations ───
void bootSequence();
bool initSensor();

// ============================================================
// SETUP
// ============================================================
void setup() {
  bootSequence();
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  // Read debounced switch
  DeviceMode mode = switchRead();

  // Detect mode change
  if (switchChanged()) {
    switchAckChange();
    currentMode = mode;
    Serial.print("[SWITCH] ");
    Serial.println(modeName(currentMode));

    // Update LED to reflect new mode
    if (sensorOK) {
      if (currentMode == MODE_REGISTER) {
        ledRegisterIdle();
      } else {
        ledRecognizeReady();
      }
    }
  }

  // Milestone 1: just idle here.
  // Future milestones add finger detection, registration, recognition.
  delay(50);
}

// ============================================================
// BOOT SEQUENCE
// ============================================================
void bootSequence() {
  // 1. Serial init (wait up to 5s for USB CDC)
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { delay(10); }
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.print("[BOOT] Fingerprint Unlocker v");
  Serial.println(FW_VERSION);
  Serial.println("========================================");
  Serial.println("[BOOT] Serial OK");

  // 2. Switch init (debounced)
  switchInit();
  currentMode = switchRead();
  Serial.print("[BOOT] Switch: ");
  Serial.println(modeName(currentMode));

  // 3. Sensor init
  sensorOK = initSensor();

  if (sensorOK) {
    // Init LED wrappers
    ledInit(&fingerprint);

    // Boot OK flash, then set mode LED
    ledBootOK();
    Serial.println("[BOOT] LED: breathing blue (boot OK)");

    delay(2000);  // show boot LED for 2s

    // Set LED to current mode
    if (currentMode == MODE_REGISTER) {
      ledRegisterIdle();
      Serial.println("[MODE] REGISTER");
    } else {
      ledRecognizeReady();
      Serial.println("[MODE] RECOGNIZE");
    }
  }

  Serial.println("[BOOT] Ready");
  Serial.println("----------------------------------------");
}

// ============================================================
// SENSOR INIT
// ============================================================
bool initSensor() {
  Serial.println("[BOOT] Starting UART1...");
  Serial1.begin(SENSOR_BAUD);
  delay(SENSOR_INIT_DELAY_MS);
  Serial.println("[BOOT] UART1 OK");

  Serial.print("[BOOT] Sensor init... ");
  Serial.flush();

  int result = fingerprint.begin(Serial1);

  if (result != 0) {
    Serial.print("FAILED (code: ");
    Serial.print(result);
    Serial.println(")");
    Serial.println("[ERROR] Sensor init failed — halting");

    // Can't use ledInit() since sensor failed, so drive LED directly
    // (may or may not work depending on failure mode)
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDRed, 0);

    // Halt — no point continuing without sensor
    while (true) { delay(1000); }
    return false;  // never reached
  }

  Serial.println("OK");

  // Print sensor info
  Serial.print("[BOOT] Enrolled fingerprints: ");
  Serial.println(fingerprint.getEnrollCount());

  return true;
}
