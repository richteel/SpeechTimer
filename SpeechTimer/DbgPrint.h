#ifndef DBGPRINT
#define DBGPRINT

#define DEBUG 1          // SET TO 0 OUT TO REMOVE TRACES
#define USE_PICOPROBE 1  // Set to 1 if using Picoprobe

#if DEBUG
#if USE_PICOPROBE
#define Dbg_begin(...) Serial1.begin(__VA_ARGS__);
#define Dbg_end(...) Serial1.end(__VA_ARGS__);
#define Dbg_print(...) Serial1.print(__VA_ARGS__)
#define Dbg_println(...) Serial1.println(__VA_ARGS__)
#define Dbg_printf(...) Serial1.printf(__VA_ARGS__)
#define Dbg_printlnf(...) Serial1.printlnf(__VA_ARGS__)
#define Dbg_write(...) Serial1.write(__VA_ARGS__)
#else
#define Dbg_begin(...) Serial.begin(__VA_ARGS__);
#define Dbg_end(...) Serial.end(__VA_ARGS__);
#define Dbg_print(...) Serial.print(__VA_ARGS__)
#define Dbg_println(...) Serial.println(__VA_ARGS__)
#define Dbg_printf(...) Serial.printf(__VA_ARGS__)
#define Dbg_printlnf(...) Serial.printlnf(__VA_ARGS__)
#define Dbg_write(...) Serial.write(__VA_ARGS__)
#endif
#else
#define Dbg_begin(...)
#define Dbg_end(...)
#define Dbg_print(...)
#define Dbg_println(...)
#define Dbg_printf(...)
#define Dbg_printlnf(...)
#define Dbg_write(...)
#endif

#endif  // DBGPRINT