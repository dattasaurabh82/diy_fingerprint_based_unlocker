// ============================================================
// led_feedback.h — LED ring semantic state wrappers
// Header-only module (implementation inline)
// ============================================================
#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include <DFRobot_ID809.h>

// ─── State ───
static DFRobot_ID809* _led_fp = nullptr;

// ─── Init ───
// Call once after fingerprint.begin() succeeds.
inline void ledInit(DFRobot_ID809* fp) { _led_fp = fp; }

// ─── Internal helper (uses library enum types, not uint8_t) ───
static inline void _ledCtrl(DFRobot_ID809::eLEDMode_t mode, DFRobot_ID809::eLEDColor_t color, uint8_t count) {
  if (_led_fp) _led_fp->ctrlLED(mode, color, count);
}

// ─── Boot ───
inline void ledBootOK()          { _ledCtrl(DFRobot_ID809::eBreathing,  DFRobot_ID809::eLEDBlue,   3); }
inline void ledSensorFail()      { _ledCtrl(DFRobot_ID809::eKeepsOn,    DFRobot_ID809::eLEDRed,    0); }

// ─── Register mode ───
inline void ledRegisterIdle()    { _ledCtrl(DFRobot_ID809::eBreathing,  DFRobot_ID809::eLEDYellow, 0); }
inline void ledWaitingFinger()   { _ledCtrl(DFRobot_ID809::eBreathing,  DFRobot_ID809::eLEDYellow, 0); }
inline void ledCaptureOK()       { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDGreen,  3); }
inline void ledCaptureFail()     { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDRed,    3); }
inline void ledWaitingPassword() { _ledCtrl(DFRobot_ID809::eBreathing,  DFRobot_ID809::eLEDCyan,   0); }
inline void ledRegisterSuccess() { _ledCtrl(DFRobot_ID809::eKeepsOn,    DFRobot_ID809::eLEDGreen,  0); }
inline void ledRegisterFail()    { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDRed,    3); }

// ─── Recognize mode ───
inline void ledRecognizeReady()  { _ledCtrl(DFRobot_ID809::eBreathing,  DFRobot_ID809::eLEDBlue,   0); }
inline void ledMatchFound()      { _ledCtrl(DFRobot_ID809::eKeepsOn,    DFRobot_ID809::eLEDGreen,  0); }
inline void ledNoMatch()         { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDRed,    3); }
inline void ledNoRegistration()  { _ledCtrl(DFRobot_ID809::eKeepsOn,    DFRobot_ID809::eLEDRed,    0); }
inline void ledCooldown()        { _ledCtrl(DFRobot_ID809::eSlowBlink,  DFRobot_ID809::eLEDGreen,  0); }

// ─── System ───
inline void ledSwitchAbort()     { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDCyan,   3); }
inline void ledCorruptState()    { _ledCtrl(DFRobot_ID809::eFastBlink,  DFRobot_ID809::eLEDRed,    5); }
inline void ledOff()             { _ledCtrl(DFRobot_ID809::eNormalClose, DFRobot_ID809::eLEDBlue,   0); }

#endif // LED_FEEDBACK_H
