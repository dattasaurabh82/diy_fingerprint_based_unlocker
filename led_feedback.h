// ============================================================
// led_feedback.h — LED ring state wrappers
// ============================================================
#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include <DFRobot_ID809.h>

// Must be called once after fingerprint.begin() succeeds.
// Stores a pointer to the fingerprint object for LED control.
void ledInit(DFRobot_ID809* fp);

// ─── Semantic LED states ───
// Each maps to a specific mode/color/count combination.

void ledBootOK();           // Breathing Blue ×3  — boot succeeded
void ledSensorFail();       // Solid Red           — sensor init failed (stays on)

void ledRegisterIdle();     // Breathing Yellow    — REGISTER mode, waiting
void ledWaitingFinger();    // Breathing Yellow    — waiting for finger placement
void ledCaptureOK();        // Fast Blink Green ×3 — finger captured successfully
void ledCaptureFail();      // Fast Blink Red ×3   — capture failed
void ledWaitingPassword();  // Breathing Cyan      — waiting for serial password input
void ledRegisterSuccess();  // Solid Green         — registration completed
void ledRegisterFail();     // Fast Blink Red ×3   — registration failed

void ledRecognizeReady();   // Breathing Blue      — RECOGNIZE mode, ready
void ledMatchFound();       // Solid Green         — fingerprint matched
void ledNoMatch();          // Fast Blink Red ×3   — no match
void ledNoRegistration();   // Solid Red           — no registration exists
void ledCooldown();         // Slow Blink Green    — cooldown active

void ledSwitchAbort();      // Fast Blink Purple ×3 — switch changed, aborting
void ledCorruptState();     // Fast Blink Red ×5   — corrupt state detected

void ledOff();              // LED off

#endif // LED_FEEDBACK_H
