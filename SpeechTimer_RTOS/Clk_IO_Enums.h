#ifndef CLK_IO_ENUMS_H
#define CLK_IO_ENUMS_H

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

#endif  // CLK_IO_ENUMS_H