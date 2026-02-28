// TEST 4: Capacitive touch detection
// Expected: "Finger detected!" when touching, "No finger" when not

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
  Serial.println("TEST 4: Finger Detection");
  Serial.println("================================");
  Serial.println("Touch the sensor to test...\n");

  // Set LED to breathing blue as "ready" indicator
  fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDBlue, 0);
}

void loop() {
  uint8_t result = fingerprint.detectFinger();

  if (result == 1) {
    Serial.println(">>> FINGER DETECTED <<<");
    fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDGreen, 0);
  } else {
    Serial.println("    (no finger)");
    fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDBlue, 0);
  }

  delay(500);
}