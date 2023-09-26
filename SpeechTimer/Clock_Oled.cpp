#include "Clock_Oled.h"


// ***** PUBLIC *****
// Constructor:
Clock_Oled::Clock_Oled(uint8_t w, uint8_t h, TwoWire *twi, int8_t rst_pin, uint32_t clkDuring, uint32_t clkAfter) {
  _display = Adafruit_SSD1306(w, h, twi, rst_pin, clkDuring, clkAfter);
}

bool Clock_Oled::begin(uint8_t switchvcc, uint8_t i2caddr, bool reset, bool periphBegin) {
  if (!_display.begin(switchvcc, i2caddr, reset, periphBegin)) {
    Serial.println(F("SSD1306 allocation failed"));
    return _displaySetupInit;
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  _display.display();

  // Clear the buffer
  _display.clearDisplay();

  drawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5);

  _display.display();

  _displaySetupInit = true;

  return _displaySetupInit;
}

void Clock_Oled::clear() {
  _display.clearDisplay();
}

void Clock_Oled::drawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t boarderWidth, uint16_t color) {
  if (boarderWidth > 0) {
    for (uint8_t i = 0; i < boarderWidth; i++) {
      if (width - (2 * i) > 0 && height - (2 * i) > 0) {
        _display.drawRect(x + i, y + i, width - (2 * i), height - (2 * i), color);
      } else {
        break;
      }
    }
  } else {
    _display.fillRect(x, y, width, height, color);
  }
}

void Clock_Oled::updateTime(const char *time) {
}


// ***** PRIVATE *****