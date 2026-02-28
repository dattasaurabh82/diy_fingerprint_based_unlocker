// ============================================================
// DIY Fingerprint-Based Unlocker
// Milestone 2: Boot + Switch + Sensor + Finger Detection
// ============================================================

#include <DFRobot_ID809.h>

#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"

// ─── Globals ───
DFRobot_ID809 fingerprint;
bool sensorOK = false;
DeviceMode currentMode = MODE_RECOGNIZE;

// Finger detection state — avoid spamming serial
bool fingerPresent = false;
bool fingerPrevious = false;

// ─── Forward declarations ───
void bootSequence();
bool initSensor();
void handleModeSwitch();
void pollFinger();

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
  // 1. Check for mode switch change
  handleModeSwitch();

  // 2. Poll finger detection (mode-aware)
  if (sensorOK) {
    pollFinger();
  }

  delay(50);
}

// ============================================================
// MODE SWITCH HANDLER
// ============================================================
void handleModeSwitch() {
  switchRead();

  if (switchChanged()) {
    switchAckChange();
    currentMode = switchRead();
    Serial.print("[SWITCH] ");
    Serial.println(modeName(currentMode));

    // Reset finger state on mode change
    fingerPresent = false;
    fingerPrevious = false;

    // Update LED to reflect new mode
    if (sensorOK) {
      if (currentMode == MODE_REGISTER) {
        ledRegisterIdle();
      } else {
        ledRecognizeReady();
      }
    }
  }
}

// ============================================================
// FINGER DETECTION (edge-triggered serial, mode-aware LED)
// ============================================================
void pollFinger() {
  fingerPresent = (fingerprint.detectFinger() == 1);

  // Only print on transitions (edge-triggered, not level)
  if (fingerPresent && !fingerPrevious) {
    // Finger just placed
    Serial.println("[SENSOR] Finger detected");

    if (currentMode == MODE_REGISTER) {
      // In register mode: green flash acknowledges touch
      // (full enrollment flow comes in M3)
      ledCaptureOK();
      Serial.println("[SENSOR] (REGISTER mode — enrollment not yet implemented)");
    } else {
      // In recognize mode: attempt capture + search
      // (full recognition flow comes in M4)
      ledMatchFound();

      uint8_t ret = fingerprint.collectionFingerprint(MATCH_TIMEOUT);
      if (ret != ERR_ID809) {
        uint8_t matchID = fingerprint.search();
        if (matchID != 0 && matchID != ERR_ID809) {
          Serial.print("[SENSOR] Match! ID #");
          Serial.println(matchID);
          ledMatchFound();
        } else {
          Serial.println("[SENSOR] No match");
          ledNoMatch();
        }
      } else {
        Serial.println("[SENSOR] Capture failed");
        ledNoMatch();
      }

      delay(1500);  // brief pause to show result LED

      // Restore mode LED
      ledRecognizeReady();
    }
  } else if (!fingerPresent && fingerPrevious) {
    // Finger just lifted
    Serial.println("[SENSOR] Finger removed");

    // Restore mode LED after register touch
    if (currentMode == MODE_REGISTER) {
      ledRegisterIdle();
    }
  }

  fingerPrevious = fingerPresent;
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

  bool ok = fingerprint.begin(Serial1);

  if (!ok) {
    Serial.println("FAILED");
    Serial.println("[ERROR] Sensor init failed — halting");

    // Can't use ledInit() since sensor failed, so drive LED directly
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDRed, 0);

    // Halt — no point continuing without sensor
    while (true) { delay(1000); }
    return false;
  }

  Serial.println("OK");

  // Print sensor info
  Serial.print("[BOOT] Enrolled fingerprints: ");
  Serial.println(fingerprint.getEnrollCount());

  return true;
}
