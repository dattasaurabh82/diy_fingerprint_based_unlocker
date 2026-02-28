// TEST 3: LED Ring control
// Expected: LED cycles through colors with different modes

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
  Serial.println("TEST 3: LED Ring Control");
  Serial.println("================================");

  Serial.println("Breathing BLUE...");
  fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDBlue, 3);
  delay(4000);

  Serial.println("Fast blink GREEN...");
  fingerprint.ctrlLED(fingerprint.eFastBlink, fingerprint.eLEDGreen, 3);
  delay(3000);

  Serial.println("Slow blink RED...");
  fingerprint.ctrlLED(fingerprint.eFastBlink, fingerprint.eLEDRed, 3);
  delay(4000);

  Serial.println("Always ON CYAN...");
  fingerprint.ctrlLED(fingerprint.eKeepsOn, fingerprint.eLEDCyan, 0);
  delay(3000);

  Serial.println("LED OFF");
  fingerprint.ctrlLED(fingerprint.eNormalClose, fingerprint.eLEDBlue, 0);

  Serial.println("\n✓ LED test complete!");
}

void loop() {
  delay(1000);
}