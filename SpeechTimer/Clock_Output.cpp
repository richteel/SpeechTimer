#include "Clock_Output.h"

// ***** PUBLIC *****
Clock_Output::Clock_Output() {
}

bool Clock_Output::begin() {
  _clock = Clock_Clock();
  _clock.begin();
  _oleDisplay.begin();

  return true;
}

void Clock_Output::updateIpAddress(const char *ipAddress) {
  _oleDisplay.updateIpAddress(ipAddress);
}

void Clock_Output::updateTime(const char *time) {
  if (strlen(time) > 0) {
    _clock.updateTime(time);
    _oleDisplay.updateTime(time);
  }
}


// ***** PRIVATE *****
