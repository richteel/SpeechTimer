#include "Clk_Output.h"

// ***** PUBLIC *****
Clk_Output::Clk_Output(Clk_SdCard *sdcard) {
  // _strip must be created in the header. I thought that was bad practice but it is the only way it works.
  // _strip = Adafruit_NeoPixel(NEOPIXEL_TOTAL_PIXELS, PIN_NEOPIXEL_DATA, NEOPIXEL_TYPE);
  // The following three lines work if Adafruit_NeoPixel _strip = Adafruit_NeoPixel(NULL); is in the header
  // but it produces several warnings.
  // _strip.updateType(NEOPIXEL_TYPE);
  // _strip.updateLength(NEOPIXEL_TOTAL_PIXELS);
  // _strip.setPin(PIN_NEOPIXEL_DATA);
  
  // Validate pointer parameter
  if (sdcard == nullptr) {
    _sdcard = nullptr;
    return;
  }
  
  _sdcard = sdcard;
  _clockColor = _strip.Color(255, 255, 255);
  _timerColor = _strip.Color(255, 255, 255);
}

bool Clk_Output::begin() {
  pinMode(PIN_DETECTOR_LIGHT, INPUT);
  int lightLevel = analogRead(PIN_DETECTOR_LIGHT);

  // Initialize rolling average array and cached sum
  _lightValueSum = 0;
  for (int i = 0; i < _lightValuesLen; i++) {
    _lightValues[i] = lightLevel;
    _lightValueSum += lightLevel;
  }

  _strip.begin();            // INITIALIZE NeoPixel strip object (REQUIRED)
  _strip.show();             // Turn OFF all pixels ASAP
  _strip.setBrightness(50);  // Set BRIGHTNESS to about 1/5 (max = 255)

  if (!_oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_SCREEN_ADDRESS, true, true)) {
    return false;
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  _oledDisplay.display();
  // Clear the buffer
  _oledDisplay.clearDisplay();

  oledDrawRectangle(0, 0, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 5);

  char messageText[] = "Speech Timer v1";
  uint8_t fontSize = 1;
  uint8_t y = oledGetYForMiddleText(messageText, fontSize);
  oledDrawTextCentered(y, messageText, fontSize);

  _oledDisplay.display();

  // Show that the RTC is not set by showing "--:--" on the Neopixel 7-segment
  // and the OLED displays
  updateTime("--:--");

  return true;
}

uint32_t Clk_Output::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void Clk_Output::neopixelClear() {
  for (int i = 0; i < NEOPIXEL_TOTAL_PIXELS; i++) {
    _strip.setPixelColor(i, 0, 0, 0);
  }
  _strip.show();
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void Clk_Output::neopixelColorWipe(uint32_t color, int wait) {
  for (int i = 0; i < _strip.numPixels(); i++) {  // For each pixel in strip...
    _strip.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    _strip.show();                                //  Update strip to match
    delay(wait);                                  //  Pause for a moment
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void Clk_Output::neopixelRainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this loop:
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    // strip.rainbow() can take a single argument (first pixel hue) or
    // optionally a few extras: number of rainbow repetitions (default 1),
    // saturation and value (brightness) (both 0-255, similar to the
    // ColorHSV() function, default 255), and a true/false flag for whether
    // to apply gamma correction to provide 'truer' colors (default true).
    _strip.rainbow(firstPixelHue);
    // Above line is equivalent to:
    // strip.rainbow(firstPixelHue, 1, 255, 255, true);
    _strip.show();  // Update strip with new contents
    delay(wait);    // Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void Clk_Output::neopixelTheaterChase(uint32_t color, int wait) {
  for (int a = 0; a < 10; a++) {   // Repeat 10 times...
    for (int b = 0; b < 3; b++) {  //  'b' counts from 0 to 2...
      _strip.clear();              //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for (int c = b; c < _strip.numPixels(); c += 3) {
        _strip.setPixelColor(c, color);  // Set pixel 'c' to value 'color'
      }
      _strip.show();  // Update strip with new contents
      delay(wait);    // Pause for a moment
    }
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void Clk_Output::neopixelTheaterChaseRainbow(int wait) {
  int firstPixelHue = 0;           // First pixel starts at red (hue 0)
  for (int a = 0; a < 30; a++) {   // Repeat 30 times...
    for (int b = 0; b < 3; b++) {  //  'b' counts from 0 to 2...
      _strip.clear();              //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for (int c = b; c < _strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int hue = firstPixelHue + c * 65536L / _strip.numPixels();
        uint32_t color = _strip.gamma32(_strip.ColorHSV(hue));  // hue -> RGB
        _strip.setPixelColor(c, color);                         // Set pixel 'c' to value 'color'
      }
      _strip.show();                // Update strip with new contents
      delay(wait);                  // Pause for a moment
      firstPixelHue += 65536 / 90;  // One cycle of color wheel over 90 frames
    }
  }
}

void Clk_Output::oledClear() {
  _oledDisplay.clearDisplay();
}

void Clk_Output::oledDrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t boarderWidth, uint16_t color) {
  if (boarderWidth > 0) {
    for (uint8_t i = 0; i < boarderWidth; i++) {
      if (width - (2 * i) > 0 && height - (2 * i) > 0) {
        _oledDisplay.drawRect(x + i, y + i, width - (2 * i), height - (2 * i), color);
      } else {
        break;
      }
    }
  } else {
    _oledDisplay.fillRect(x, y, width, height, color);
  }
}

void Clk_Output::oledDrawText(uint8_t x, uint8_t y, const char *message, uint8_t size, uint16_t color) {
  _oledDisplay.setTextSize(size);    // Normal 1:1 pixel scale
  _oledDisplay.setTextColor(color);  // Draw white text
  _oledDisplay.setCursor(x, y);      // Start at top-left corner
  _oledDisplay.cp437(true);          // Use full 256 char 'Code Page 437' font
  _oledDisplay.write(message);
}

void Clk_Output::oledDrawTextCentered(uint8_t y, const char *message, uint8_t size, uint16_t color) {
  uint8_t x = oledGetXForCenteredText(message, size);
  oledDrawText(x, y, message, size, color);
}

uint8_t Clk_Output::oledGetXForCenteredText(const char *message, uint8_t size) {
  uint8_t textWidth = strlen(message) * (6 * size);

  return (OLED_SCREEN_WIDTH - textWidth) / 2;
}

uint8_t Clk_Output::oledGetYForMiddleText(const char *message, uint8_t size) {
  uint8_t textHeight = (8 * size);

  return (OLED_SCREEN_HEIGHT - textHeight) / 2;
}

void Clk_Output::timerNext(bool advanceOne) {
  // Advance the array index by one if we did not just come from clock display or setting min/max times
  if (advanceOne && clockMode != ClockMode::TimerSetMin && clockMode != ClockMode::TimerSetMax) {
    _currentTimerIndex++;
    if (_currentTimerIndex >= TIMERARRAYSIZE) {
      _currentTimerIndex = 0;
    }
  }

  // If we came from srtting min/max times, go to the ready state
  if (clockMode == ClockMode::TimerSetMin || clockMode == ClockMode::TimerSetMax) {
    clockMode = ClockMode::TimerReady;
  } else if (_sdcard->sdCardConfig.timers[_currentTimerIndex].manual) {
    clockMode = ClockMode::TimerSetMin;
  }

  // Make certain that min is less than max
  // Do this if the min/max times are from the SD Card or entered from the remote
  if (_sdcard->sdCardConfig.timers[_currentTimerIndex].min > _sdcard->sdCardConfig.timers[_currentTimerIndex].max) {
    int tmp = _sdcard->sdCardConfig.timers[_currentTimerIndex].max;
    _sdcard->sdCardConfig.timers[_currentTimerIndex].max = _sdcard->sdCardConfig.timers[_currentTimerIndex].min;
    _sdcard->sdCardConfig.timers[_currentTimerIndex].min = tmp;
  }
}

int Clk_Output::timerSetMin(int minVal) {
  int _timerMin = _sdcard->sdCardConfig.timers[_currentTimerIndex].min;
  bool _timerManual = _sdcard->sdCardConfig.timers[_currentTimerIndex].manual;

  if (!_timerManual) {
    return _timerMin;
  }

  if (minVal >= 0) {
    _sdcard->sdCardConfig.timers[_currentTimerIndex].min = minVal % 100;
  }

  return _sdcard->sdCardConfig.timers[_currentTimerIndex].min;
}

int Clk_Output::timerSetMax(int maxVal) {
  int _timerMax = _sdcard->sdCardConfig.timers[_currentTimerIndex].max;
  bool _timerManual = _sdcard->sdCardConfig.timers[_currentTimerIndex].manual;

  if (!_timerManual) {
    return _timerMax;
  }

  if (maxVal >= 0) {
    _sdcard->sdCardConfig.timers[_currentTimerIndex].max = maxVal % 100;
  }

  return _sdcard->sdCardConfig.timers[_currentTimerIndex].max;
}

void Clk_Output::updateIpAddress(const char *ipAddress) {
  _ipAddress = ipAddress;
  updateOledDisplay(_lastTime);
}

void Clk_Output::updateTime(const char *time) {
  if (strlen(time) == 0) {
    return;
  }

  // *** UPDATE NEOPIXELS ***
  updateClock(time, _clockColor);

  // *** UPDATE OLED ***
  if (strcmp(_lastTime, time) != 0) {
    updateOledDisplay(time);
  }

  strcpy(_lastTime, time);
}

void Clk_Output::updateTimer() {
  int _timerMin = _sdcard->sdCardConfig.timers[_currentTimerIndex].min;
  int _timerMax = _sdcard->sdCardConfig.timers[_currentTimerIndex].max;
  // bool _timerManual = _sdcard->sdCardConfig.timers[_currentTimerIndex].manual;

  if (millis() - timerFlashPreviousMillis >= timerFlashInterval) {
    timerFlashPreviousMillis = millis();
    if (clockMode == ClockMode::TimerSetMin || clockMode == ClockMode::TimerSetMax) {
      _digitsOn = !_digitsOn;
    }
  }

  char dispBuffer[10];
  int seconds = 0;
  int minutes = 0;

  int secondsMin = _timerMin * 60;
  int secondsMax = _timerMax * 60;
  int secondsMid = (secondsMin + secondsMax) / 2;

  // Set default value of the display buffer using optimized formatting
  // Optimize: Manual formatting is faster than sprintf for simple cases
  dispBuffer[0] = '0' + (_timerMin / 10);
  dispBuffer[1] = '0' + (_timerMin % 10);
  dispBuffer[2] = ':';
  dispBuffer[3] = '0' + (_timerMax / 10);
  dispBuffer[4] = '0' + (_timerMax % 10);
  dispBuffer[5] = '\0';

  switch (clockMode) {
    case (ClockMode::TimerReady):
      _timerColor = Color(255, 255, 255);
      _timerStartMillis = 0;
      _timerEndMillis = 0;
      break;
    case (ClockMode::TimerSetMin):
      if (!_digitsOn) {
        // Optimize: Manual string construction
        dispBuffer[0] = ' ';
        dispBuffer[1] = ' ';
        // dispBuffer[2] already ':'
        // dispBuffer[3-5] already set
      }
      break;
    case (ClockMode::TimerSetMax):
      if (!_digitsOn) {
        // dispBuffer[0-2] already set
        dispBuffer[3] = ' ';
        dispBuffer[4] = ' ';
        dispBuffer[5] = '\0';
      }
      break;
    case (ClockMode::TimerRun):
      if (_timerStartMillis == 0) {
        _timerStartMillis = millis();
      }

      seconds = getTimerSeconds();

      if (seconds < secondsMin) {
        _timerColor = Color(255, 255, 255);
      } else if (seconds < secondsMid) {
        _timerColor = Color(0, 255, 0);
      } else if (seconds < secondsMax) {
        _timerColor = Color(255, 255, 0);
      } else if (seconds >= secondsMax) {
        _timerColor = Color(255, 0, 0);
      }

      minutes = seconds / 60;
      seconds = seconds % 60;
      // Optimize: Manual formatting instead of sprintf
      dispBuffer[0] = '0' + (minutes / 10);
      dispBuffer[1] = '0' + (minutes % 10);
      dispBuffer[2] = ':';
      dispBuffer[3] = '0' + (seconds / 10);
      dispBuffer[4] = '0' + (seconds % 10);
      dispBuffer[5] = '\0';
      break;
    case (ClockMode::TimerStop):
      if (_timerEndMillis == 0) {
        _timerEndMillis = millis();
      }
      seconds = getTimerSeconds();
      minutes = seconds / 60;
      seconds = seconds % 60;
      sprintf(dispBuffer, "%02d:%02d", minutes, seconds);
      break;
    default:
      break;
  }
  // updateTime(dispBuffer);
  updateClock(dispBuffer, _timerColor);
}


// ***** PRIVATE *****
uint8_t Clk_Output::getCharSegements(char c) {
  uint8_t retval = 255;
  int arayLen = sizeof(segments) / sizeof(uint8_t);

  for (int i = 0; i < arayLen; i++) {
    if (digits[i] == c) {
      retval = segments[i];
      break;
    }
  }

  return retval;
}

int Clk_Output::getTimerSeconds() {
  if (_timerStartMillis == 0 || (_timerEndMillis != 0 && _timerStartMillis > _timerEndMillis)) {
    return 0;
  }

  if (_timerEndMillis != 0) {
    return (_timerEndMillis - _timerStartMillis) / 1000;
  }

  return (millis() - _timerStartMillis) / 1000;
}

int Clk_Output::rollingAverageBrightness() {
  // unsigned long _lightReadPreviousMillis = 0;
  // const long _lightReadInterval = 500;

  if (millis() - _lightReadPreviousMillis > _lightReadInterval) {
    _lightReadPreviousMillis = millis();

    // Read Light Level and Adjust Brightness
    int lightLevel = analogRead(PIN_DETECTOR_LIGHT);

    // Optimize: Use sliding window with cached sum
    // Subtract old value from sum, add new value
    _lightValueSum -= _lightValues[_lightValueOldestIdx];
    _lightValues[_lightValueOldestIdx] = lightLevel;
    _lightValueSum += lightLevel;
    
    _lightValueOldestIdx++;
    if (_lightValueOldestIdx >= _lightValuesLen) {
      _lightValueOldestIdx = 0;
    }
  }

  // Optimize: Use cached sum instead of recalculating every time (O(1) vs O(n))
  int averageLightLevel = _lightValueSum / _lightValuesLen;
  int brightness = int((double(NEOPIXEL_MAX_BRIGHTNESS) / double(1023)) * averageLightLevel);

  if (brightness < NEOPIXEL_MIN_BRIGHTNESS) {
    brightness = NEOPIXEL_MIN_BRIGHTNESS;
  }

  return brightness;
}

void Clk_Output::updateClock(const char *time, uint32_t c) {
  int brightness = rollingAverageBrightness();

  _strip.setBrightness(brightness);

  for (int i = 0; i < 5; i++) {
    digits_clock[i] = time[i];
  }
  digits_clock[5] = '\0';

  int timeLen = strlen(digits_clock);
  int lastPixel = -1;

  for (int dig = timeLen - 1; dig >= 0; dig--) {
    if (dig != 2) {
      uint8_t digSeg = getCharSegements(digits_clock[dig]);
      uint8_t segmentMask = 0x01;

      for (int segment = 0; segment < 7; segment++) {
        bool show = ((digSeg & segmentMask) == segmentMask);
        segmentMask = segmentMask << 1;
        for (int segmentPixel = 0; segmentPixel < NEOPIXEL_NUM_PER_SEGMENT; segmentPixel++) {
          lastPixel++;
          if (digSeg == 255) {
            continue;
          }
          if (show) {
            _strip.setPixelColor(lastPixel, c);
          } else {
            _strip.setPixelColor(lastPixel, 0, 0, 0);
          }
        }
      }
    } else {
      lastPixel++;
      if (digits_clock[dig] == ':') {
        _strip.setPixelColor(lastPixel, c);
        lastPixel++;
        _strip.setPixelColor(lastPixel, c);
      } else if (digits_clock[dig] == '-') {
        _strip.setPixelColor(lastPixel, c);
        lastPixel++;
        _strip.setPixelColor(lastPixel, 0, 0, 0);
      } else if (digits_clock[dig] == '.') {
        _strip.setPixelColor(lastPixel, 0, 0, 0);
        lastPixel++;
        _strip.setPixelColor(lastPixel, c);
      } else {
        _strip.setPixelColor(lastPixel, 0, 0, 0);
        lastPixel++;
        _strip.setPixelColor(lastPixel, 0, 0, 0);
      }
    }
  }
  _strip.show();
}

void Clk_Output::updateOledDisplay(const char *time) {
  char lineBuffer[22];

  _oledDisplay.clearDisplay();
  oledDrawTextCentered(OLED_LINE_2_Y, "Current Time");
  if (strlen(_ipAddress) > 0) {
    sprintf(lineBuffer, "IP: %s\0", _ipAddress);
  } else {
    sprintf(lineBuffer, "Not Connected\0");
  }
  oledDrawTextCentered(OLED_LINE_1_Y, lineBuffer);
  sprintf(lineBuffer, "%s\0", time);
  oledDrawTextCentered(OLED_LINE_3_Y, lineBuffer);
  _oledDisplay.display();
}
