#ifndef CLOCK_OUTPUT_H
#define CLOCK_OUTPUT_H

#include "Clock_Clock.h"

class Clock_Output {
public:
  // Constructor:
  Clock_Output();

  bool begin();

  void updateTime(const char *time);
private:
  Clock_Clock _clock;
};

#endif