// ============================================================
// TEST 8: HID Modifier Key Combos
// Tests the exact key sequences needed for Mac unlock flow.
//
// !! WARNING !! This will send real keystrokes to your computer!
//
// INSTRUCTIONS:
//   1. Flash this sketch
//   2. Open Serial Monitor at 115200
//   3. Each test waits for you to type 'y' + Enter before sending
//   4. You have 5 seconds after confirming to switch focus
//
// TESTS:
//   Test A: Ctrl+Cmd+Q  (locks Mac screen)
//   Test B: LEFT_CTRL press/release x2  (wake — non-printable)
//   Test C: Cmd+A  (select-all in focused field)
//   Test D: Type a test string + Enter
//   Test E: Full sequence (lock → wake → clear → type → enter)
// ============================================================

#include <Keyboard.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) delay(10);
  delay(500);

  Keyboard.begin();

  Serial.println("========================================");
  Serial.println("TEST 8: HID Modifier Key Combos");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Each test waits for 'y' + Enter.");
  Serial.println("You get 5 seconds after confirming to prepare.");
  Serial.println();

  // ─── Test A: Ctrl+Cmd+Q (lock screen) ───
  Serial.println("--- Test A: Ctrl+Cmd+Q (WILL LOCK YOUR MAC) ---");
  Serial.println("Type 'y' to proceed, anything else to skip:");
  if (waitForConfirm()) {
    countdown(5);
    Serial.println("[HID] Sending Ctrl+Cmd+Q...");

    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('q');
    delay(50);
    Keyboard.releaseAll();

    Serial.println("[HID] Done. Did your Mac lock?");
  } else {
    Serial.println("Skipped.");
  }
  Serial.println();

  // ─── Test B: LEFT_CTRL x2 (wake — non-printable) ───
  Serial.println("--- Test B: LEFT_CTRL x2 (wake from sleep) ---");
  Serial.println("If Mac is sleeping/locked, this should wake the display.");
  Serial.println("Type 'y' to proceed:");
  if (waitForConfirm()) {
    countdown(5);
    Serial.println("[HID] Sending LEFT_CTRL x2...");

    for (int i = 0; i < 2; i++) {
      Keyboard.press(KEY_LEFT_CTRL);
      delay(50);
      Keyboard.release(KEY_LEFT_CTRL);
      delay(200);
    }

    Serial.println("[HID] Done. Did the display wake?");
  } else {
    Serial.println("Skipped.");
  }
  Serial.println();

  // ─── Test C: Cmd+A (select all) ───
  Serial.println("--- Test C: Cmd+A (select all) ---");
  Serial.println("Focus a text field before confirming.");
  Serial.println("Type 'y' to proceed:");
  if (waitForConfirm()) {
    countdown(5);
    Serial.println("[HID] Sending Cmd+A...");

    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('a');
    delay(50);
    Keyboard.releaseAll();

    Serial.println("[HID] Done. Was text selected?");
  } else {
    Serial.println("Skipped.");
  }
  Serial.println();

  // ─── Test D: Type string + Enter ───
  Serial.println("--- Test D: Type 'TestPassword123' + Enter ---");
  Serial.println("Focus a text field before confirming.");
  Serial.println("Type 'y' to proceed:");
  if (waitForConfirm()) {
    countdown(5);
    Serial.println("[HID] Typing...");

    Keyboard.print("TestPassword123");
    delay(100);
    Keyboard.press(KEY_RETURN);
    delay(50);
    Keyboard.release(KEY_RETURN);

    Serial.println("[HID] Done. Did text appear + Enter?");
  } else {
    Serial.println("Skipped.");
  }
  Serial.println();

  // ─── Test E: Full sequence ───
  Serial.println("--- Test E: FULL UNLOCK SEQUENCE ---");
  Serial.println("This does: Lock → Wait 2s → Wake → Wait 2s → Cmd+A → Type → Enter");
  Serial.println("!! This WILL lock your Mac then try to type into password field !!");
  Serial.println("Type 'y' to proceed:");
  if (waitForConfirm()) {
    countdown(5);

    // Step 1: Lock
    Serial.println("[HID] Step 1: Ctrl+Cmd+Q (lock)...");
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('q');
    delay(50);
    Keyboard.releaseAll();
    Serial.println("[HID] Waiting 2s...");
    delay(2000);

    // Step 2: Wake
    Serial.println("[HID] Step 2: LEFT_CTRL x2 (wake)...");
    for (int i = 0; i < 2; i++) {
      Keyboard.press(KEY_LEFT_CTRL);
      delay(50);
      Keyboard.release(KEY_LEFT_CTRL);
      delay(200);
    }
    Serial.println("[HID] Waiting 2s...");
    delay(2000);

    // Step 3: Select all (clear field)
    Serial.println("[HID] Step 3: Cmd+A (clear field)...");
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('a');
    delay(50);
    Keyboard.releaseAll();
    delay(200);

    // Step 4: Type password
    Serial.println("[HID] Step 4: Typing 'TestPassword123'...");
    Keyboard.print("TestPassword123");
    delay(100);

    // Step 5: Enter
    Serial.println("[HID] Step 5: Enter...");
    Keyboard.press(KEY_RETURN);
    delay(50);
    Keyboard.release(KEY_RETURN);

    Serial.println("[HID] Full sequence complete!");
    Serial.println("Did it: lock → wake → type into password field → submit?");
  } else {
    Serial.println("Skipped.");
  }

  Serial.println();
  Serial.println("========================================");
  Serial.println("All tests complete.");
  Serial.println("========================================");

  Keyboard.end();
}

void loop() {
  delay(1000);
}

// ─── Helpers ───

bool waitForConfirm() {
  // Flush any stale input
  while (Serial.available()) Serial.read();

  // Wait for input
  while (!Serial.available()) delay(50);

  String input = Serial.readStringUntil('\n');
  input.trim();
  return (input == "y" || input == "Y");
}

void countdown(int seconds) {
  for (int i = seconds; i > 0; i--) {
    Serial.print(i);
    Serial.print("... ");
    delay(1000);
  }
  Serial.println("GO!");
}
