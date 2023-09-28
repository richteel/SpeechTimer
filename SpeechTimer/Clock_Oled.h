#ifndef CLOCK_OLED_H
#define CLOCK_OLED_H

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DebugPrint.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define LINE_1_Y 2
#define LINE_2_Y 12 // 8
#define LINE_3_Y 22 // 16

class Clock_Oled {
public:
  // Constructor:
  Clock_Oled(uint8_t w = SCREEN_WIDTH, uint8_t h = SCREEN_HEIGHT, TwoWire *twi = &Wire, int8_t rst_pin = OLED_RESET, uint32_t clkDuring = 400000UL, uint32_t clkAfter = 100000UL);

  bool begin(uint8_t switchvcc = SSD1306_SWITCHCAPVCC, uint8_t i2caddr = SCREEN_ADDRESS, bool reset = true, bool periphBegin = true);

  void clear();

  void drawRectangle(uint8_t x = 0, uint8_t y = 0, uint8_t width = SCREEN_WIDTH, uint8_t height = SCREEN_HEIGHT, uint8_t boarderWidth = 0, uint16_t color = SSD1306_WHITE);

  void drawText(uint8_t x, uint8_t y, const char *message, uint8_t size = 1, uint16_t color = SSD1306_WHITE);

  void drawTextCentered(uint8_t y, const char *message, uint8_t size = 1, uint16_t color = SSD1306_WHITE);

  uint8_t getXForCenteredText(const char *message, uint8_t size = 1);

  uint8_t getYForMiddleText(const char *message, uint8_t size = 1);

  void updateIpAddress(const char *ipAddress);

  void updateTime(const char *time);
private:
  Adafruit_SSD1306 _display = Adafruit_SSD1306();
  bool _displaySetupInit = false;
  uint8_t _screenWidth;
  uint8_t _screenHeight;

  const char *_ipAddress;
  char _lastTime[22] = {};
};

#endif