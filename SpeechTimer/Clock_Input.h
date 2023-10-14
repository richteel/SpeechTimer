#ifndef CLOCK_INPUT_H
#define CLOCK_INPUT_H

#include "ClockIOenums.h"
#include "Clock_SdCard.h"
#include "Clock_Wifi.h"
#include "Clock_Rtc.h"
#include "Clock_Remote.h"
#include "DebugPrint.h"

struct InputAction {
  ClockMode mode;
  ModeActions action;
  unsigned int value01;
  unsigned int value02;
  unsigned int value03;
};

#define clockBufferSize 10

class Clock_Input {
public:
  InputAction input_buffer[clockBufferSize] = {};

  // Constructor:
  Clock_Input(Clock_SdCard *sdcard, Clock_Wifi *clock_Wifi);

  bool begin();

  bool checkInputs();

private:
  Clock_SdCard *_sdcard;
  Clock_Wifi *_clock_Wifi;
  Clock_Remote _remote;

  void addToBuffer(InputAction inputaction);
  InputAction getFromBuffer();
};

#endif