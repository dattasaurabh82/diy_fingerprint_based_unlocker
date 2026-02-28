// TEST 1: Auto-detect sensor baud rate and establish communication
// Expected: Finds sensor at 115200 (default), prints baud rate

#include <DFRobot_ID809.h>

#define FPSerial Serial1  // UART0: GPIO0=TX, GPIO1=RX

DFRobot_ID809 fingerprint;

uint32_t baudRates[] = { 9600, 19200, 38400, 57600, 115200 };
uint8_t numRates = 5;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("================================");
  Serial.println("TEST 1: Auto-Detect Baud Rate");
  Serial.println("================================");

  bool found = false;

  for (uint8_t i = 0; i < numRates; i++) {
    Serial.print("Trying ");
    Serial.print(baudRates[i]);
    Serial.print(" baud... ");

    FPSerial.begin(baudRates[i]);
    delay(100);

    if (fingerprint.begin(FPSerial)) {
      Serial.println("FOUND!");
      Serial.print(">>> Sensor detected at ");
      Serial.print(baudRates[i]);
      Serial.println(" baud <<<");
      found = true;
      break;
    } else {
      Serial.println("no response");
      FPSerial.end();
    }
  }

  if (!found) {
    Serial.println("\n✗ SENSOR NOT FOUND at any baud rate!");
    Serial.println("Troubleshooting:");
    Serial.println("  1. Swap Yellow(TX) and Black(RX) wires");
    Serial.println("  2. Check power (Green+White → 3V3, Red → GND)");
    Serial.println("  3. Check solder joints / connector seating");
    Serial.println("  4. Try powering sensor from 5V VBUS instead of 3V3");
  }
}

void loop() {
  delay(1000);
}