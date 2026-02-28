// ============================================================
// DIY Fingerprint-Based Unlocker
// Milestone 3: Boot + Switch + Sensor + Registration Flow
// ============================================================

#include <DFRobot_ID809.h>

#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"
#include "eeprom_storage.h"
#include "registration.h"

// ─── Globals ───
DFRobot_ID809 fingerprint;
bool sensorOK = false;
DeviceMode currentMode = MODE_RECOGNIZE;

// Finger detection state (RECOGNIZE mode only in M3)
bool fingerPresent = false;
bool fingerPrevious = false;

// ─── Forward declarations ───
void bootSequence();
bool initSensor();
void handleModeSwitch();
void handleRegisterMode();
void handleRecognizeMode();

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

  // 2. Mode-specific behavior
  if (sensorOK) {
    if (currentMode == MODE_REGISTER) {
      handleRegisterMode();
    } else {
      handleRecognizeMode();
    }
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
// REGISTER MODE — wait for finger touch to start registration
// ============================================================
void handleRegisterMode() {
  fingerPresent = (fingerprint.detectFinger() == 1);

  if (fingerPresent && !fingerPrevious) {
    Serial.println("[SENSOR] Finger detected — starting registration");

    // Run the full registration flow (blocks until complete or failed)
    bool success = runRegistration(fingerprint);

    if (success) {
      Serial.println("[REG] Success — flip switch to RECOGNIZE to use");
    } else {
      Serial.println("[REG] Registration did not complete");
      // Check if switch changed during registration
      switchRead();
      if (switchChanged()) {
        switchAckChange();
        currentMode = switchRead();
        Serial.print("[SWITCH] ");
        Serial.println(modeName(currentMode));
        if (currentMode == MODE_RECOGNIZE) {
          ledRecognizeReady();
        }
        fingerPrevious = false;
        fingerPresent = false;
        return;
      }
    }

    // Restore register idle LED
    ledRegisterIdle();

    // Wait for finger removal before allowing another attempt
    while (fingerprint.detectFinger()) delay(100);
    fingerPresent = false;
    fingerPrevious = false;
    return;
  }

  fingerPrevious = fingerPresent;
}

// ============================================================
// RECOGNIZE MODE — detect + match (placeholder HID in M4)
// ============================================================
void handleRecognizeMode() {
  fingerPresent = (fingerprint.detectFinger() == 1);

  if (fingerPresent && !fingerPrevious) {
    Serial.println("[SENSOR] Finger detected");
    ledMatchFound();

    uint8_t ret = fingerprint.collectionFingerprint(MATCH_TIMEOUT);
    if (ret != ERR_ID809) {
      uint8_t matchID = fingerprint.search();
      if (matchID != 0 && matchID != ERR_ID809) {
        Serial.print("[AUTH] Match! ID #");
        Serial.println(matchID);

        // Read password to confirm EEPROM is paired
        if (eepromHasRegistration()) {
          Serial.println("[AUTH] Registration valid — HID unlock (M4)");
        } else {
          Serial.println("[AUTH] No password in EEPROM — flip to REGISTER");
        }

        ledMatchFound();
      } else {
        Serial.println("[AUTH] No match");
        ledNoMatch();
      }
    } else {
      Serial.println("[AUTH] Capture failed");
      ledNoMatch();
    }

    delay(1500);
    ledRecognizeReady();

    // Wait for finger removal
    while (fingerprint.detectFinger()) delay(100);
    fingerPresent = false;
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

    // 4. EEPROM init
    eepromInit();
    if (eepromHasRegistration()) {
      Serial.print("[BOOT] EEPROM: valid registration (slot ");
      Serial.print(eepromGetActiveSlot());
      Serial.println(")");
    } else {
      Serial.println("[BOOT] EEPROM: no registration (virgin)");
    }

    // Boot OK flash, then set mode LED
    ledBootOK();
    Serial.println("[BOOT] LED: breathing blue (boot OK)");
    delay(2000);

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
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDRed, 0);
    while (true) { delay(1000); }
    return false;
  }

  Serial.println("OK");

  Serial.print("[BOOT] Enrolled fingerprints: ");
  Serial.println(fingerprint.getEnrollCount());

  return true;
}
