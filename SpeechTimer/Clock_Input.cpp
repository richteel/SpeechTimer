#include "Clock_Input.h"

// Clock_Remote remote = Clock_Remote();

// ***** PUBLIC *****
Clock_Input::Clock_Input(Clock_SdCard *sdcard, Clock_Wifi *clock_Wifi) {
  _sdcard = sdcard;
  _clock_Wifi = clock_Wifi;
  _remote = Clock_Remote();
}

bool Clock_Input::begin() {
  _remote.begin();

  return true;
}

bool Clock_Input::checkInputs() {
  char recvChar = _remote.checkForButton();

  if (recvChar != '\0') {
    D_println(recvChar);
    InputAction bufferItem = new InputAction();

    switch (recvChar) {
      case '*':
        bufferItem.action = ClockMode::Timer;
        break;
      case '#':
        break;
      default:
        break;
    }
  }

  return true;
}


// ***** PRIVATE *****
void Clock_Input::addToBuffer(InputAction inputaction) {
}

InputAction Clock_Input::getFromBuffer() {
}
