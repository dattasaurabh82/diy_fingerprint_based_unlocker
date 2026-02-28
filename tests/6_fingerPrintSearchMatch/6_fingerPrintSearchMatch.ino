// TEST 6: Search for a fingerprint match
// Expected: Identifies enrolled prints, rejects unknown prints

#include <DFRobot_ID809.h>

#define FPSerial Serial1

DFRobot_ID809 fingerprint;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  FPSerial.begin(115200);

  if (!fingerprint.begin(FPSerial)) {
    Serial.println("Sensor not found!");
    while (1) delay(1000);
  }

  Serial.println("================================");
  Serial.println("TEST 6: Fingerprint Search");
  Serial.println("================================");

  uint8_t count = fingerprint.getEnrollCount();
  Serial.print("Enrolled fingerprints: ");
  Serial.println(count);

  if (count == 0) {
    Serial.println("No fingerprints enrolled! Run Test 5 first.");
    while (1) delay(1000);
  }

  Serial.println("Place finger on sensor to search...\n");
}

void loop() {
  // Ready indicator
  fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDBlue, 0);

  // Wait for finger
  if (fingerprint.detectFinger()) {

    // Capture
    uint8_t ret = fingerprint.collectionFingerprint(5);
    if (ret != ERR_ID809) {

      // Search against all enrolled prints
      uint8_t matchID = fingerprint.search();

      if (matchID != 0 && matchID != ERR_ID809) {
        // MATCH FOUND
        fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDGreen, 0);
        Serial.print("✓ MATCH! Fingerprint ID #");
        Serial.println(matchID);
      } else {
        // NO MATCH
        fingerprint.ctrlLED(fingerprint.eFastBlink, fingerprint.eLEDRed, 3);
        Serial.println("✗ No match — unknown fingerprint");
      }
    } else {
      Serial.println("Capture failed, try again");
    }

    // Wait for finger removal
    while (fingerprint.detectFinger()) delay(100);
    delay(1000);
  }

  delay(200);
}