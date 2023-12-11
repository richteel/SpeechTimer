#include "Clk_Remote.h"
// See: https://forum.arduino.cc/t/irremote-library-causing-multiple-definition-error/950269
/*
 * This include defines the actual pin number for pins like IR_RECEIVE_PIN, IR_SEND_PIN for many different boards and architectures
 */
#include "RemotePinDefinitions.h"
#include <IRremote.hpp>  // include the library

// ***** PUBLIC *****
Clk_Remote::Clk_Remote() {
}

bool Clk_Remote::begin() {
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  return true;
}

char Clk_Remote::checkForButton() {
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
char Clk_Remote::getButton(unsigned int command) {
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
