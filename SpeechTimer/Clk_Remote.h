#ifndef CLK_REMOTE_H
#define CLK_REMOTE_H

#include <cstdint>  // Required for uint8_t, uint16_t, etc. used by IRremote

/*
 * Specify which protocol(s) should be used for decoding.
 * If no protocol is defined, all protocols (except Bang&Olufsen) are active.
 * This must be done before the #include <IRremote.hpp>
 */
//#define DECODE_DENON        // Includes Sharp
//#define DECODE_JVC
//#define DECODE_KASEIKYO
//#define DECODE_PANASONIC    // alias for DECODE_KASEIKYO
//#define DECODE_LG
#define DECODE_NEC  // Includes Apple and Onkyo
//#define DECODE_SAMSUNG
//#define DECODE_SONY
//#define DECODE_RC5
//#define DECODE_RC6

//#define DECODE_BOSEWAVE
//#define DECODE_LEGO_PF
//#define DECODE_MAGIQUEST
//#define DECODE_WHYNTER
//#define DECODE_FAST

//#define DECODE_DISTANCE_WIDTH // Universal decoder for pulse distance width protocols
//#define DECODE_HASH         // special decoder for all protocols

//#define DECODE_BEO          // This protocol must always be enabled manually, i.e. it is NOT enabled if no protocol is defined. It prevents decoding of SONY!

//#define DEBUG               // Activate this for lots of lovely debug output from the decoders.

//#define RAW_BUFFER_LENGTH  180  // Default is 112 if DECODE_MAGIQUEST is enabled, otherwise 100.

// #include <Arduino.h>

#include "DbgPrint.h"   // Serial helpers

class Clk_Remote {
public:
  // Constructor:
  Clk_Remote();

  bool begin();

  char checkForButton();

private:
  static constexpr char _buttons[17]{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'U', 'D', 'L', 'R', 'K' };
  static constexpr unsigned int _commands[17]{ 0x19, 0x45, 0x46, 0x47, 0x44, 0x40, 0x43, 0x07, 0x15, 0x09, 0x16, 0x0D, 0x18, 0x52, 0x08, 0x5A, 0x1C };

  char getButton(unsigned int command);
};

#endif  // CLK_REMOTE_H