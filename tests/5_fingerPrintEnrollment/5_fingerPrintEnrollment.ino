// TEST 5: Enroll a fingerprint
// Expected: Guides you through 3 presses, stores fingerprint

#include <DFRobot_ID809.h>

#define FPSerial Serial1
#define COLLECT_NUMBER 3  // 3 samples for enrollment

DFRobot_ID809 fingerprint;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  FPSerial.begin(115200);

  if (!fingerprint.begin(FPSerial)) {
    Serial.println("Sensor not found!");
    while (1) delay(1000);
  }

  Serial.println("=================================");
  Serial.println("TEST 5: Fingerprint Enrollment");
  Serial.println("=================================");

  // Find an empty slot
  uint8_t ID = fingerprint.getEmptyID();
  if (ID == ERR_ID809) {
    Serial.println("Error getting empty ID! Storage may be full.");
    while (1) delay(1000);
  }

  Serial.print("Will enroll to slot #");
  Serial.println(ID);
  Serial.println();

  // Collect fingerprint 3 times
  for (uint8_t i = 0; i < COLLECT_NUMBER; i++) {
    Serial.print("Step ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(COLLECT_NUMBER);
    Serial.println(": Place your finger on the sensor...");

    // Breathing yellow = waiting for finger
    fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDYellow, 0);

    // Wait for finger with 10-second timeout
    uint8_t ret = fingerprint.collectionFingerprint(10);

    if (ret != ERR_ID809) {
      // Success — flash green
      fingerprint.ctrlLED(fingerprint.eFastBlink, fingerprint.eLEDGreen, 3);
      Serial.println("   ✓ Fingerprint captured!");

      Serial.println("   Remove your finger...");
      while (fingerprint.detectFinger()) delay(100);
      delay(500);
    } else {
      // Failure — flash red
      fingerprint.ctrlLED(fingerprint.eFastBlink, fingerprint.eLEDRed, 3);
      Serial.println("   ✗ Capture failed! Retrying...");
      i--;  // retry this step
      delay(1000);
    }
  }

  // Store the fingerprint
  Serial.print("Storing fingerprint as ID #");
  Serial.print(ID);
  Serial.print("... ");

  uint8_t ret = fingerprint.storeFingerprint(ID);
  if (ret == 0) {
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDGreen, 0);
    Serial.println("SUCCESS!");
    Serial.print("Total enrolled fingerprints: ");
    Serial.println(fingerprint.getEnrollCount());
  } else {
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDRed, 0);
    Serial.print("FAILED! Error: ");
    Serial.println(fingerprint.getErrorDescription());
  }

  delay(2000);
  fingerprint.ctrlLED(fingerprint.eNormalClose, fingerprint.eLEDBlue, 0);
}

void loop() {
  delay(1000);
}