// ============================================================
// MILESTONE 1b: Add sensor init back
// ============================================================

#include <DFRobot_ID809.h>

#define PIN_SENSOR_TX   0
#define PIN_SENSOR_RX   1
#define PIN_MODE_SWITCH 3

DFRobot_ID809 fingerprint;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { delay(10); }
  delay(500);

  Serial.println();
  Serial.println("[BOOT] === Milestone 1b — with sensor ===");
  Serial.println("[BOOT] Serial OK");

  pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);
  Serial.print("[BOOT] Switch: ");
  Serial.println(digitalRead(PIN_MODE_SWITCH) == LOW ? "LOW (REGISTER)" : "HIGH (RECOGNIZE)");

  Serial.println("[BOOT] Starting UART1...");
  Serial1.begin(115200);  // defaults to GPIO0=TX, GPIO1=RX
  delay(200);  // let sensor wake up
  Serial.println("[BOOT] UART1 OK");

  Serial.println("[BOOT] Calling fingerprint.begin()...");
  Serial.flush();
  int result = fingerprint.begin(Serial1);
  Serial.print("[BOOT] fingerprint.begin() returned: ");
  Serial.println(result);

  if (result != 0) {
    Serial.println("[ERROR] Sensor init failed!");
  } else {
    Serial.println("[BOOT] Sensor OK");
    // Quick LED test to confirm comms
    fingerprint.ctrlLED(fingerprint.eBreathing, fingerprint.eLEDBlue, 0);
    Serial.println("[BOOT] LED set to breathing blue");
  }

  Serial.println("[BOOT] Ready");
}

void loop() {
  static int lastState = -1;
  int state = digitalRead(PIN_MODE_SWITCH);
  if (state != lastState) {
    lastState = state;
    Serial.print("[SWITCH] ");
    Serial.println(state == LOW ? "REGISTER" : "RECOGNIZE");
  }
  delay(100);
}
