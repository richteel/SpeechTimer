#include "Clock_Output.h"

// ***** PUBLIC *****
Clock_Output::Clock_Output() {
}

bool Clock_Output::begin() {
  _clock = Clock_Clock();
  _clock.begin();

  return true;
}

void Clock_Output::updateTime(const char *time) {
  _clock.updateTime(time);
}


// ***** PRIVATE *****
