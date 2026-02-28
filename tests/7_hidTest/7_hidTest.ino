#include <Keyboard.h>

void setup() {
  Keyboard.begin();
  delay(3000);  // wait 3 seconds so the OS has time to recognize the USB keyboard

  Keyboard.print("Hello from RP2350-Zero!");

  Keyboard.end();
}

void loop() {
  // nothing — one-shot Hello from RP2350-Zero!Hello from RP2350-Zero!
}