#include <DFRobot_ID809.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "switch_control.h"
#include "led_feedback.h"
#include "eeprom_storage.h"
#include "crypto.h"
#include "irq_finger.h"
#include "registration.h"
#include "hid_unlock.h"
#include "recognition.h"
#include "validation.h"

// ─── Globals ───
DFRobot_ID809 fingerprint;
bool sensorOK = false;
DeviceMode currentMode = MODE_RECOGNIZE;
BootState bootState = BOOT_VIRGIN;

// ─── Serial command buffer ───
static String _serialCmdBuf = "";

// ─── Forward declarations ───
void bootSequence();
bool initSensor();
void handleSerialCommands();
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
  // 1. Check for serial commands (e.g. !RESET from Web Serial UI)
  handleSerialCommands();

  // 2. Check for mode switch change
  handleModeSwitch();

  // 3. Mode-specific behavior
  if (sensorOK) {
    if (currentMode == MODE_REGISTER) {
      handleRegisterMode();
    } else {
      handleRecognizeMode();
    }
  }

  delay(100);  // IRQ-driven — no need for fast polling
}

// ============================================================
// SERIAL COMMAND HANDLER
// ============================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      _serialCmdBuf.trim();
      if (_serialCmdBuf == "!RESET") {
        Serial.println("[CMD] Rebooting...");
        Serial.flush();
        delay(100);  // let the response reach the host
        watchdog_reboot(0, 0, 0);  // immediate hardware reset
        while (true) { tight_loop_contents(); }  // wait for watchdog
      }
      // Future commands can be added here with else-if
      _serialCmdBuf = "";
    } else {
      if (_serialCmdBuf.length() < 32) {  // prevent runaway buffer
        _serialCmdBuf += c;
      }
    }
  }
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

    // Clear any pending IRQ trigger from before the switch
    irqFingerClear();

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
// REGISTER MODE — wait for IRQ touch to start registration
// ============================================================
void handleRegisterMode() {
  if (irqFingerDetected()) {
    Serial.println("[SENSOR] Finger detected (IRQ) — starting registration");

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
        irqFingerClear();
        return;
      }
    }

    // Restore register idle LED
    ledRegisterIdle();

    // Wait for finger removal before allowing another IRQ trigger
    while (fingerprint.detectFinger()) delay(100);
    irqFingerClear();  // discard any IRQ that fired during removal wait
    return;
  }
}

// ============================================================
// RECOGNIZE MODE — IRQ detect + match + HID unlock
// ============================================================
void handleRecognizeMode() {
  if (irqFingerDetected()) {
    Serial.println("[SENSOR] Finger detected (IRQ)");

    // Run recognition (capture → match → HID unlock)
    bool unlocked = runRecognition(fingerprint);

    if (unlocked) {
      // After cooldown LED phase, return to ready
      ledRecognizeReady();
    }
    // If not unlocked (no match, capture fail, etc.), LED already reset in runRecognition

    // Wait for finger removal
    while (fingerprint.detectFinger()) delay(100);
    irqFingerClear();  // discard any IRQ that fired during removal wait
  }
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

    // 4. Crypto init (derive device-bound AES key from unique ID)
    cryptoInit();

    // 5. EEPROM init
    eepromInit();

    // 6. HID keyboard init
    hidInit();
    Serial.println("[BOOT] HID Keyboard OK");
    Serial.flush();

    // 7. IRQ finger detection init
    irqFingerInit();
    Serial.flush();

    // 8. Boot integrity validation
    bootState = runBootValidation(fingerprint);
    Serial.flush();

    // Boot OK flash
    ledBootOK();
    delay(2000);

    // 9. Decide initial mode based on validation result
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
