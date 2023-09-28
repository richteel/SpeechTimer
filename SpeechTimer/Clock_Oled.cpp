#include "Clock_Oled.h"


// ***** PUBLIC *****
// Constructor:
Clock_Oled::Clock_Oled(uint8_t w, uint8_t h, TwoWire *twi, int8_t rst_pin, uint32_t clkDuring, uint32_t clkAfter) {
  _screenWidth = w;
  _screenHeight = h;
  _display = Adafruit_SSD1306(w, h, twi, rst_pin, clkDuring, clkAfter);
}

bool Clock_Oled::begin(uint8_t switchvcc, uint8_t i2caddr, bool reset, bool periphBegin) {
  if (!_display.begin(switchvcc, i2caddr, reset, periphBegin)) {
    D_println(F("SSD1306 allocation failed"));
    return _displaySetupInit;
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  _display.display();

  // Clear the buffer
  _display.clearDisplay();

  drawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5);

  char messageText[] = "Speech Timer v1";
  uint8_t fontSize = 1;
  uint8_t y = getYForMiddleText(messageText, fontSize);
  drawTextCentered(y, messageText, fontSize);
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

void Clock_Oled::drawText(uint8_t x, uint8_t y, const char *message, uint8_t size, uint16_t color) {
  _display.setTextSize(size);    // Normal 1:1 pixel scale
  _display.setTextColor(color);  // Draw white text
  _display.setCursor(x, y);      // Start at top-left corner
  _display.cp437(true);          // Use full 256 char 'Code Page 437' font
  _display.write(message);
}

void Clock_Oled::drawTextCentered(uint8_t y, const char *message, uint8_t size, uint16_t color) {
  uint8_t x = getXForCenteredText(message, size);
  drawText(x, y, message, size, color);
}

uint8_t Clock_Oled::getXForCenteredText(const char *message, uint8_t size) {
  uint8_t textWidth = strlen(message) * (6 * size);

  return (_screenWidth - textWidth) / 2;
}

uint8_t Clock_Oled::getYForMiddleText(const char *message, uint8_t size) {
  uint8_t textHeight = (8 * size);

  return (_screenHeight - textHeight) / 2;
}

void Clock_Oled::updateIpAddress(const char *ipAddress) {
  _ipAddress = ipAddress;
}

void Clock_Oled::updateTime(const char *time) {
  if (strcmp(_lastTime, time) == 0) {
    return;
  }

  char lineBuffer[22];
  strcpy(_lastTime, time);

  _display.clearDisplay();
  drawTextCentered(LINE_2_Y, "Current Time");
  sprintf(lineBuffer, "IP: %s\0", _ipAddress);
  drawTextCentered(LINE_1_Y, lineBuffer);
  sprintf(lineBuffer, "%s\0", time);
  drawTextCentered(LINE_3_Y, lineBuffer);
  _display.display();
  // delay(250);
}


// ***** PRIVATE *****