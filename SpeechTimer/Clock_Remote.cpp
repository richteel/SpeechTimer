#include "Clock_Remote.h"
// See: https://forum.arduino.cc/t/irremote-library-causing-multiple-definition-error/950269
/*
 * This include defines the actual pin number for pins like IR_RECEIVE_PIN, IR_SEND_PIN for many different boards and architectures
 */
#include "PinDefinitionsAndMore.h"
#include <IRremote.hpp>  // include the library

// ***** PUBLIC *****
Clock_Remote::Clock_Remote() {
}

bool Clock_Remote::begin() {
  // D_println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  D_print(F("Ready to receive IR signals of protocols: "));
  if (Serial) {
    printActiveIRProtocols(&Serial);
  } else if (Serial1) {
    printActiveIRProtocols(&Serial1);
  }
  D_println(F("at pin " STR(IR_RECEIVE_PIN)));

  return true;
}

void Clock_Remote::checkIrRecv() {
  if (IrReceiver.decode()) {
    // if (Serial) {
    //   IrReceiver.printIRResultShort(&Serial);
    //   IrReceiver.printIRSendUsage(&Serial);
    // } else if (Serial1) {
    //   IrReceiver.printIRResultShort(&Serial1);
    //   IrReceiver.printIRSendUsage(&Serial1);
    // }

    // if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
    //   D_println(F("Received noise or an unknown (or not yet enabled) protocol"));
    //   // We have an unknown protocol here, print more info

    //   if (Serial) {
    //     IrReceiver.printIRResultRawFormatted(&Serial, true);
    //   } else if (Serial1) {
    //     IrReceiver.printIRResultRawFormatted(&Serial1, true);
    //   }
    // }

    // D_printf("Command is %d with type of %s\n", IrReceiver.decodedIRData.command, "uint16_t");

    if (IrReceiver.decodedIRData.protocol == NEC && IrReceiver.decodedIRData.address == 0) {
      char button = getButton(IrReceiver.decodedIRData.command);
      if (button != '\0') {
        D_println();
        D_printf("The button pressed was %c\n", button);
        D_printf("Repeated %d times\n", IrReceiver.decodedIRData.flags == IRDATA_FLAGS_IS_REPEAT);
      }
    }

    /*
         * !!!Important!!! Enable receiving of the next value,
         * since receiving has stopped after the end of the current received data packet.
         */
    IrReceiver.resume();  // Enable receiving of the next value
  }
}

char Clock_Remote::checkForButton() {
  char retval = '\0';

  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC && IrReceiver.decodedIRData.address == 0) {
      char button = getButton(IrReceiver.decodedIRData.command);
      if (button != '\0' && IrReceiver.decodedIRData.flags != IRDATA_FLAGS_IS_REPEAT) {
        retval = button;
      }
    }

    /*
         * !!!Important!!! Enable receiving of the next value,
         * since receiving has stopped after the end of the current received data packet.
         */
    IrReceiver.resume();  // Enable receiving of the next value
  }

  return retval;
}


// ***** PRIVATE *****
char Clock_Remote::getButton(unsigned int command) {
  char retval = '\0';
  int l = sizeof(_commands) / sizeof(_commands[0]);

  for (int i = 0; i < l; i++) {
    if (command == _commands[i]) {
      retval = _buttons[i];
      break;
    }
  }

  return retval;
}
