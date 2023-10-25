#ifndef GLOBALDEFINES_H
#define GLOBALDEFINES_H

/*
  ** HW Device Connection on Raspberry Pi Pico W **
    Pin GPIO  Device  Device Pin/Function
    6   GP4   OLED      SDA
    7   GP5   OLED      SCK
    20  GP15  Indicator LED
    21  GP16  SD Card   MISO
    22  GP17  SD Card   CS
    24  GP18  SD Card   SCK
    25  GP19  SD Card   MOSI
    26  GP20  SD Card   CD
    29  GP22  IR LED
    31  GP26  IR Remote Signal
    32  GP27  Light Detector (CdS Photoresistor)
    34  GP28  NeoPixels Data
*/

// ***** Pins *****
#define PIN_OLED_SDA 4
#define PIN_OLED_SCK 5
#define PIN_LED_INDICATOR 15
#define PIN_SDCARD_MISO 16
#define PIN_SDCARD_CS 17
#define PIN_SDCARD_SCK 18
#define PIN_SDCARD_MOSI 19
#define PIN_SDCARD_CD 20
#define PIN_LED_IR 22
#define PIN_DETECTOR_IR 26
#define PIN_DETECTOR_LIGHT 27
#define PIN_NEOPIXEL_DATA 28

// ***** OLED SETTINGS *****
#define OLED_SCREEN_WIDTH 128  // OLED display width, in pixels
#define OLED_SCREEN_HEIGHT 32  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define OLED_LINE_1_Y 2
#define OLED_LINE_2_Y 12 // 8
#define OLED_LINE_3_Y 22 // 16

// ***** NEOPIXEL CLOCK SETTINGS *****
#define NEOPIXEL_TOTAL_PIXELS 58
#define NEOPIXEL_TYPE NEO_RGB + NEO_KHZ800
#define NEOPIXEL_NUM_PER_SEGMENT 2
#define NEOPIXEL_NUM_DIGITS 4
#define NEOPIXEL_NUM_COLONS 2
#define NEOPIXEL_INITIAL_BRIGHTNESS 50





#endif  // GLOBALDEFINES_H