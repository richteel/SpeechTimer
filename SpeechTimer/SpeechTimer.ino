/*
 SpeechTimer.ino

  AUTHOR: Richard Teel
  DATE: 16 September 2023
  HW TESTED: Raspberry Pi Pico W

  Speech Timer for Toastmasters Club or any event that needs to display a timer with minutes and seconds.
  ** Features **
    - Clock is displayed if there is an internet connection and the timer is not running.
    - Web interface for operation and configuration.
    - IR Remote for simpler control with speech timer presets of:
      - 1 to 2 minutes for Table Topics
      - 5 to 7 minutes for Speech (default)
      - 2 to 3 minutes for Evaluation
      - Ability to manually set a timer
    - Timer digits change color based on timer settings
      - Digits are white when started
      - Digits are green when the minimum time is reached
      - Digits are yellow when the halfway between the minimum and maximum times
      - Digits are red when the maximum time is reached and stay red
  
  ** Issues/Future Improvements **
  These items are known and planed to be worked on in the future but there is no guarantee that any of 
  them will be addressed.
    - When starting, it may take a minute or longer before the timer function is available. This is due 
      to the time it takes to attempt to set up a Wi-Fi connection. Startup may happen as quickly as 10 
      seconds but may last up to 2 minutes.
    - An auxiliary clock display is planned to allow the same information to be displayed with one 
      positioned for the speaker and the other for the timer/audience.
  
  ** HW Device Connection on Raspberry Pi Pico W **
    Pin GPIO  Device  Device Pin/Function
    6   GP4   OLED      SDA
    7   GP5   OLED      SCK
    21  GP16  SD Card   MISO
    22  GP17  SD Card   CS
    24  GP18  SD Card   SCK
    25  GP19  SD Card   MOSI
    26  GP20  SD Card   CD
    31  GP26  IR Remote Signal
    34  GP28  NeoPixels Data

  Power
    NeoPixels - GND and VBUS (Pin 40)
    SD Card   - GND and 3V (Pin 36)
    OLED      - GND and 3V (Pin 36)
    IR Remote - GND and 3V (Pin 36)
*/


#include <Arduino.h>
#include "tests.h"
#include "DebugPrint.h"
#include "Clock_SdCard.h"

void setup() {
  D_SerialBegin(115200);

  /*
    If DEBUG=1 in DebugPring.h, we are debugging the code so
    wait for the serial connection with the PC. This will cause
    the code to halt until a terminal is connected, but only if
    we are debugging the code.
  */
  while (DEBUG && !Serial)
    ;
  
  runTests();
  D_println("--- DONE ---");
}

void loop() {
  
}