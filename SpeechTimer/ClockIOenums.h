#ifndef CLOCKIOENUMS_H
#define CLOCKIOENUMS_H

enum class ClockMode {
  Clock,
  Timer,
  Setting,
  Test
};

enum class ModeActions {
  SetMinTime,
  SetMaxTime,
  SetColor,
  TestColors,
  TestRainbow
};

#endif