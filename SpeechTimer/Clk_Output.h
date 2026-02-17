#ifndef CLK_OUTPUT_H
#define CLK_OUTPUT_H

#include "DbgPrint.h"  // Serial helpers
#include "Defines.h"

#include <Adafruit_NeoPixel.h>
#include "StructsAndEnums.h"
#include "config.h"
#include "Clk_SdCard.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Clk_Output {
public:
  // Constructor:
  Clk_Output(Clk_SdCard *sdcard);

  bool begin();

  uint32_t Color(uint8_t r, uint8_t g, uint8_t b);

  void neopixelClear();

  void neopixelColorWipe(uint32_t color, int wait);

  void neopixelRainbow(int wait);

  void neopixelTheaterChase(uint32_t color, int wait);

  void neopixelTheaterChaseRainbow(int wait);

  void oledClear();

  void oledDrawRectangle(uint8_t x = 0, uint8_t y = 0, uint8_t width = OLED_SCREEN_WIDTH, uint8_t height = OLED_SCREEN_HEIGHT, uint8_t boarderWidth = 0, uint16_t color = SSD1306_WHITE);

  void oledDrawText(uint8_t x, uint8_t y, const char *message, uint8_t size = 1, uint16_t color = SSD1306_WHITE);

  void oledDrawTextCentered(uint8_t y, const char *message, uint8_t size = 1, uint16_t color = SSD1306_WHITE);

  uint8_t oledGetXForCenteredText(const char *message, uint8_t size = 1);

  uint8_t oledGetYForMiddleText(const char *message, uint8_t size = 1);

  void timerNext(bool advanceOne = true);

  int timerSetMax(int minVal = -1);

  int timerSetMin(int maxVal = -1);

  void setClockColor(uint8_t red, uint8_t green, uint8_t blue);

  void getClockColor(uint8_t& red, uint8_t& green, uint8_t& blue);

  void updateIpAddress(const char *ipAddress);

  void updateTime(const char *time);

  void updateTimer();

  void updateTestMode();

  void resetTestMode() { _testModeStartMillis = 0; _testStep = 0; }

  void setWiFiConnectedWaitingForTime();

  void setSdCardError(bool hasError);

  ClockMode clockMode = ClockMode::Clock;

private:
  /*
    - digits: A string of the characters that may be displayed on the 7-segment displays.
      Size is one more than the number of characters as a null char (\0) is automatically appended to the end.
    - segments: An array, which is the same as the number of chars in digits. Each value represents the segments
      for the character, which segment a being the most segnificant bit and g being the least significant digit.
      |-a-|
      f   b
      |-g-|
      e   c
      |-d-|
      - 0: 0011 1111  0x3F
      - 1: 0000 0110  0x06
      - 2: 0101 1011  0x5B
      - 3: 0100 1111  0x4F
      - 4: 0110 0110  0x66
      - 5: 0110 1101  0x6D
      - 6: 0111 1101  0x7D
      - 7: 0000 0111  0x07
      - 8: 0111 1111  0x7F
      - 9: 0110 0111  0x67
      -  : 0000 0000  0x00
      - -: 0100 0000  0x40
  */
  static constexpr char digits[13]{ "0123456789 -" };
  static constexpr uint8_t segments[12]{ 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67, 0x00, 0x40 };
  char digits_clock[6]{ "--:--" };
  Adafruit_NeoPixel _strip = Adafruit_NeoPixel(NEOPIXEL_TOTAL_PIXELS, PIN_NEOPIXEL_DATA, NEOPIXEL_TYPE);
  ClockMode _mode = ClockMode::Clock;
  uint32_t _clockColor;
  uint32_t _timerColor;
  bool _timeHasBeenSynced = false;
  Adafruit_SSD1306 _oledDisplay = Adafruit_SSD1306(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET, 400000UL, 100000UL);

  uint8_t getCharSegements(char c);

  int getTimerSeconds();

  int rollingAverageBrightness();

  void updateClock(const char *time, uint32_t c);

  void updateOledDisplay(const char *time);

  const char *_ipAddress;
  char _lastTime[22] = {};

  Clk_SdCard *_sdcard;
  int _currentTimerIndex = 0;
  unsigned long timerFlashPreviousMillis = 0;
  const long timerFlashInterval = 500;
  bool _digitsOn = true;
  unsigned long _timerStartMillis = 0;
  unsigned long _timerEndMillis = 0;
  static const int _lightValuesLen = 20;
  int _lightValues[_lightValuesLen];
  int _lightValueOldestIdx = 0;
  long _lightValueSum = 0;  // Cached sum for O(1) average calculation
  unsigned long _lightReadPreviousMillis = 0;
  
  // Test mode state
  unsigned long _testModeStartMillis = 0;
  int _testStep = 0;  // Cycles through different test colors/animations
  const long _lightReadInterval = 500;
};

#endif  // CLK_OUTPUT_H