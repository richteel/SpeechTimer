#include "Clock_Clock.h"
#include <Adafruit_NeoPixel.h>

// ***** PUBLIC *****
Clock_Clock::Clock_Clock(uint16_t totalpixels, int16_t pin, neoPixelType t, int pixelsPerSegment, int numberOfDigits, int numberOfColons) {
  _totalpixels = totalpixels;
  _pin = pin;
  _type = t;
  _pixelsPerSegment = pixelsPerSegment;
  _numberOfDigits = numberOfDigits;
  _numberOfColons = numberOfColons;

  _strip.updateType(_type);
  _strip.updateLength(_totalpixels);
  _strip.setPin(_pin);
  // Argument 1 = Number of pixels in NeoPixel strip
  // Argument 2 = Arduino pin number (most are valid)
  // Argument 3 = Pixel type flags, add together as needed:
  //   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
  //   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
  //   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
  //   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
  //   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
}

bool Clock_Clock::begin(uint8_t brightness) {
  _strip.begin();                    // INITIALIZE NeoPixel strip object (REQUIRED)
  _strip.show();                     // Turn OFF all pixels ASAP
  _strip.setBrightness(brightness);  // Set BRIGHTNESS to about 1/5 (max = 255)

  clear();
  
  updateTime("--:--");

  return true;
}

void Clock_Clock::clear() {
  for (int i = 0; i < _totalpixels; i++) {
    _strip.setPixelColor(i, 0, 0, 0);
  }
  _strip.show();
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void Clock_Clock::colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < _strip.numPixels(); i++) {  // For each pixel in strip...
    _strip.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    _strip.show();                                //  Update strip to match
    delay(wait);                                  //  Pause for a moment
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void Clock_Clock::rainbow(int wait) {
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
void Clock_Clock::theaterChase(uint32_t color, int wait) {
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
void Clock_Clock::theaterChaseRainbow(int wait) {
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

void Clock_Clock::updateTime(const char *time) {
  updateClock(time, _clockColor);
}


// ***** PRIVATE *****
uint8_t Clock_Clock::getCharSegements(char c) {
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

void Clock_Clock::updateClock(const char *time, uint32_t c) {
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
        for (int segmentPixel = 0; segmentPixel < _pixelsPerSegment; segmentPixel++) {
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
