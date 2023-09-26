#ifndef CLOCK_OUTPUT_H
#define CLOCK_OUTPUT_H

#include "Clock_Clock.h"
#include "Clock_Oled.h"

class Clock_Output {
public:
  // Constructor:
  Clock_Output();

  bool begin();

  void updateTime(const char *time);
private:
  Clock_Clock _clock;
  Clock_Oled _oleDisplay;
};

#endif