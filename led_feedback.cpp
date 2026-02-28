// ============================================================
// led_feedback.cpp — LED ring state wrappers
// ============================================================

#include "led_feedback.h"

static DFRobot_ID809* _fp = nullptr;

void ledInit(DFRobot_ID809* fp) {
  _fp = fp;
}

// Safety: no-op if sensor not initialized
static inline void _ctrl(uint8_t mode, uint8_t color, uint8_t count) {
  if (_fp) {
    _fp->ctrlLED(mode, color, count);
  }
}

// ─── Boot ───
void ledBootOK()         { _ctrl(DFRobot_ID809::eBreathing, DFRobot_ID809::eLEDBlue, 3); }
void ledSensorFail()     { _ctrl(DFRobot_ID809::eKeepsOn,   DFRobot_ID809::eLEDRed,  0); }

// ─── Register ───
void ledRegisterIdle()   { _ctrl(DFRobot_ID809::eBreathing, DFRobot_ID809::eLEDYellow, 0); }
void ledWaitingFinger()  { _ctrl(DFRobot_ID809::eBreathing, DFRobot_ID809::eLEDYellow, 0); }
void ledCaptureOK()      { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDGreen,  3); }
void ledCaptureFail()    { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDRed,    3); }
void ledWaitingPassword(){ _ctrl(DFRobot_ID809::eBreathing, DFRobot_ID809::eLEDCyan,   0); }
void ledRegisterSuccess(){ _ctrl(DFRobot_ID809::eKeepsOn,   DFRobot_ID809::eLEDGreen,  0); }
void ledRegisterFail()   { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDRed,    3); }

// ─── Recognize ───
void ledRecognizeReady() { _ctrl(DFRobot_ID809::eBreathing, DFRobot_ID809::eLEDBlue,   0); }
void ledMatchFound()     { _ctrl(DFRobot_ID809::eKeepsOn,   DFRobot_ID809::eLEDGreen,  0); }
void ledNoMatch()        { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDRed,    3); }
void ledNoRegistration() { _ctrl(DFRobot_ID809::eKeepsOn,   DFRobot_ID809::eLEDRed,    0); }
void ledCooldown()       { _ctrl(DFRobot_ID809::eSlowBlink, DFRobot_ID809::eLEDGreen,  0); }

// ─── System ───
void ledSwitchAbort()    { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDPurple, 3); }
void ledCorruptState()   { _ctrl(DFRobot_ID809::eFastBlink, DFRobot_ID809::eLEDRed,    5); }
void ledOff()            { _ctrl(DFRobot_ID809::eNormalClose,DFRobot_ID809::eLEDBlue,   0); }
