#ifndef CLOCK_CLOCK_H
#define CLOCK_CLOCK_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

enum ClockMode {
  Clock,
  Timer,
  Setting
};

class Clock_Clock {
public:
  // Constructor:
  Clock_Clock(uint16_t totalpixels = 58, int16_t pin = 28, neoPixelType t = NEO_RGB + NEO_KHZ800, int pixelsPerSegment = 2, int numberOfDigits = 4, int numberOfColons = 1);

  bool begin(uint8_t brightness = 50);

  void clear();

  void colorWipe(uint32_t color, int wait);

  void rainbow(int wait);

  void theaterChase(uint32_t color, int wait);

  void theaterChaseRainbow(int wait);

  void updateTime(const char *time);
private:
  // static constexpr int NUM_OF_PIXELS{ 58 };
  static constexpr char digits[13]{ "0123456789 -" };
  static constexpr uint8_t segments[12]{ 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67, 0x00, 0x40 };
  char digits_clock[6]{ "--:--" };
  uint16_t _totalpixels;
  int16_t _pin;
  neoPixelType _type;
  int _pixelsPerSegment;
  int _numberOfDigits;
  int _numberOfColons;
  Adafruit_NeoPixel _strip = Adafruit_NeoPixel();
  ClockMode _mode = Clock;
  uint32_t _clockColor = _strip.Color(255, 255, 255);
  uint32_t _timerColor = _strip.Color(255, 255, 255);

  uint8_t getCharSegements(char c);

  void updateClock(const char *time, uint32_t c);
};

#endif