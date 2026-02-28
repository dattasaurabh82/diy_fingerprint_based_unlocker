// ============================================================
// TEST 8: Full HID Unlock Sequence + Serial debug
//
// Pico SDK USB stack supports Serial + Keyboard together.
// If Serial Monitor doesn't connect, re-select port after upload.
//
// Flip switch to LOW → runs the complete sequence:
//   1. Ctrl+Cmd+Q  (lock)        → wait 2s
//   2. LEFT_CTRL ×2 (wake)       → wait 2s
//   3. Cmd+A (clear field)       → wait 200ms
//   4. Type "test123" + Enter
//
// LED: Blue=ready, Yellow=running, Green=done
// Serial: prints each step as it happens
// ============================================================

#include <Keyboard.h>
#include <DFRobot_ID809.h>

#define PIN_SWITCH 3

DFRobot_ID809 fp;
bool triggered = false;

void setup() {
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  // Serial — may need port re-select after upload
  Serial.begin(115200);
  while (!Serial && millis() < 5000) delay(10);
  delay(500);

  // Sensor for LED feedback
  Serial1.begin(115200);
  delay(200);
  fp.begin(Serial1);

  // HID keyboard
  Keyboard.begin();

  fp.ctrlLED(fp.eBreathing, fp.eLEDBlue, 0);

  Serial.println("========================================");
  Serial.println("TEST 8: HID Unlock Sequence");
  Serial.println("========================================");
  Serial.println("Flip switch to LOW to trigger sequence.");
  Serial.println("LED: Blue=ready, Yellow=running, Green=done");
  Serial.println();
}

void loop() {
  if (triggered) {
    delay(100);
    return;
  }

  if (digitalRead(PIN_SWITCH) == LOW) {
    delay(100);  // debounce
    if (digitalRead(PIN_SWITCH) == LOW) {
      triggered = true;

      fp.ctrlLED(fp.eKeepsOn, fp.eLEDYellow, 0);
      Serial.println("[HID] Sequence triggered — 3s prep...");
      delay(3000);

      // Step 1: Lock
      Serial.println("[HID] Step 1: Ctrl+Cmd+Q (lock)");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_LEFT_GUI);
      Keyboard.press('q');
      delay(50);
      Keyboard.releaseAll();
      Serial.println("[HID] Waiting 2s...");
      delay(2000);

      // Step 2: Wake
      Serial.println("[HID] Step 2: LEFT_CTRL x2 (wake)");
      for (int i = 0; i < 2; i++) {
        Keyboard.press(KEY_LEFT_CTRL);
        delay(50);
        Keyboard.release(KEY_LEFT_CTRL);
        delay(200);
      }
      Serial.println("[HID] Waiting 2s...");
      delay(2000);

      // Step 3: Clear field
      Serial.println("[HID] Step 3: Cmd+A (clear field)");
      Keyboard.press(KEY_LEFT_GUI);
      Keyboard.press('a');
      delay(50);
      Keyboard.releaseAll();
      delay(200);

      // Step 4: Type + Enter
      Serial.println("[HID] Step 4: Typing 'test123' + Enter");
      Keyboard.print("test123");
      delay(100);
      Keyboard.press(KEY_RETURN);
      delay(50);
      Keyboard.release(KEY_RETURN);

      delay(500);
      fp.ctrlLED(fp.eKeepsOn, fp.eLEDGreen, 0);
      Serial.println("[HID] Sequence complete!");
      Serial.println();
      Serial.println("Did it: lock -> wake -> type 'test123' -> enter?");
    }
  }

  delay(50);
}
