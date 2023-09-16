#ifndef DEBUGPRINT
#define DEBUGPRINT

#define DEBUG 1  // SET TO 0 OUT TO REMOVE TRACES

#if DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__);
#define D_SerialEnd(...) Serial.end(__VA_ARGS__);
#define D_print(...) Serial.print(__VA_ARGS__)
#define D_println(...) Serial.println(__VA_ARGS__)
#define D_printf(...) Serial.printf(__VA_ARGS__)
#define D_printlnf(...) Serial.printlnf(__VA_ARGS__)
#define D_write(...) Serial.write(__VA_ARGS__)
#else
#define D_SerialBegin(...)
#define D_SerialEnd(...)
#define D_print(...)
#define D_println(...)
#define D_printf(...)
#define D_printlnf(...)
#define D_write(...)
#endif

#endif