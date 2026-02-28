// TEST 2: Query all device parameters
// Expected: Prints device ID, security level, enrolled count, etc.

#include <DFRobot_ID809.h>

#define FPSerial Serial1

DFRobot_ID809 fingerprint;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  FPSerial.begin(115200);

  Serial.println("================================");
  Serial.println("TEST 2: Device Info Query");
  Serial.println("================================");

  if (!fingerprint.begin(FPSerial)) {
    Serial.println("✗ Sensor not found! Run Test 3 first.");
    while (1) delay(1000);
  }

  Serial.println("✓ Sensor connected!\n");

  // Connection test
  if (fingerprint.isConnected()) {
    Serial.println("isConnected(): TRUE");
  } else {
    Serial.println("isConnected(): FALSE — communication issue");
  }

  // Device info
  Serial.print("Device ID: ");
  Serial.println(fingerprint.getDeviceID());

  Serial.print("Security Level: ");
  Serial.println(fingerprint.getSecurityLevel());

  Serial.print("Baud Rate setting: ");
  Serial.println(fingerprint.getBaudrate());

  Serial.print("Duplication Check: ");
  Serial.println(fingerprint.getDuplicationCheck() ? "ON" : "OFF");

  Serial.print("Self-Learn: ");
  Serial.println(fingerprint.getSelfLearn() ? "ON" : "OFF");

  Serial.print("Enrolled fingerprints: ");
  Serial.println(fingerprint.getEnrollCount());

  Serial.print("Device Info: ");
  Serial.println(fingerprint.getDeviceInfo());

  Serial.print("Module SN: ");
  Serial.println(fingerprint.getModuleSN());

  Serial.println("\n✓ All queries complete!");
}

void loop() {
  delay(1000);
}