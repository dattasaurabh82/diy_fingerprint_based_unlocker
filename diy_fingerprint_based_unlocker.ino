// ============================================================
// DIY Fingerprint-Based Unlocker
// Milestone 5: Full build with boot validation + atomic safety
// ============================================================

#include <DFRobot_ID809.h>

#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"
#include "eeprom_storage.h"
#include "registration.h"
#include "hid_unlock.h"
#include "recognition.h"
#include "validation.h"

// ─── Globals ───
DFRobot_ID809 fingerprint;
bool sensorOK = false;
DeviceMode currentMode = MODE_RECOGNIZE;
BootState bootState = BOOT_VIRGIN;

// Finger detection state (edge-triggered)
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

    // Reset recognition state (cooldown, etc.)
    recReset();

    // Update LED to reflect new mode
    if (sensorOK) {
      if (currentMode == MODE_REGISTER) {
        ledRegisterIdle();
      } else {
        // Check registration when entering recognize mode
        if (recCheckRegistration(fingerprint)) {
          ledRecognizeReady();
        } else {
          ledNoRegistration();
          Serial.println("[AUTH] No registration — flip to REGISTER first");
        }
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
      // Update boot state now that we have a valid registration
      bootState = BOOT_VALID;
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
          recReset();
          if (recCheckRegistration(fingerprint)) {
            ledRecognizeReady();
          } else {
            ledNoRegistration();
          }
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
// RECOGNIZE MODE — detect + match + HID unlock
// ============================================================
void handleRecognizeMode() {
  fingerPresent = (fingerprint.detectFinger() == 1);

  if (fingerPresent && !fingerPrevious) {
    Serial.println("[SENSOR] Finger detected");

    // Run recognition (capture → match → HID unlock)
    bool unlocked = runRecognition(fingerprint);

    if (unlocked) {
      // After cooldown LED phase, return to ready
      ledRecognizeReady();
    }
    // If not unlocked (no match, capture fail, etc.), LED already reset in runRecognition

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

    // 5. HID keyboard init
    hidInit();
    Serial.println("[BOOT] HID Keyboard OK");
    Serial.flush();

    // 6. Boot integrity validation
    bootState = runBootValidation(fingerprint);
    Serial.flush();

    // Boot OK flash
    ledBootOK();
    delay(2000);

    // 7. Decide initial mode based on validation result
    switch (bootState) {
      case BOOT_VALID:
        // Normal operation — use switch position
        if (currentMode == MODE_REGISTER) {
          ledRegisterIdle();
          Serial.println("[MODE] REGISTER");
        } else {
          if (recCheckRegistration(fingerprint)) {
            ledRecognizeReady();
            Serial.println("[MODE] RECOGNIZE");
          } else {
            // Shouldn't happen if BOOT_VALID, but be safe
            ledNoRegistration();
            Serial.println("[MODE] RECOGNIZE (registration check failed)");
          }
        }
        break;

      case BOOT_VIRGIN:
      case BOOT_CORRUPT:
        // Force REGISTER mode regardless of switch
        currentMode = MODE_REGISTER;
        ledRegisterIdle();
        if (bootState == BOOT_VIRGIN) {
          Serial.println("[MODE] REGISTER (forced — virgin device)");
        } else {
          Serial.println("[MODE] REGISTER (forced — state was corrupt, cleaned up)");
        }
        Serial.println("[BOOT] Touch sensor to begin registration");
        break;
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
  Serial.flush();

  return true;
}
