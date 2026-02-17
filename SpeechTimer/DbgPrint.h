#ifndef DBGPRINT
#define DBGPRINT

#define DEBUG 1          // SET TO 0 TO REMOVE TRACES
#define USE_PICOPROBE 1  // Set to 1 if using Picoprobe

#if DEBUG
  // Select debug serial port based on USE_PICOPROBE
  #if USE_PICOPROBE
    #define DEBUG_SERIAL Serial1
  #else
    #define DEBUG_SERIAL Serial
  #endif
  
  // Debug macros - single abstraction layer
  #define Dbg_begin(...) DEBUG_SERIAL.begin(__VA_ARGS__)
  #define Dbg_end(...) DEBUG_SERIAL.end(__VA_ARGS__)
  #define Dbg_print(...) DEBUG_SERIAL.print(__VA_ARGS__)
  #define Dbg_println(...) DEBUG_SERIAL.println(__VA_ARGS__)
  #define Dbg_printf(...) DEBUG_SERIAL.printf(__VA_ARGS__)
  #define Dbg_write(...) DEBUG_SERIAL.write(__VA_ARGS__)
  
  // Verbose debug macro for detailed logging (custom name to avoid Arduino core conflict)
  #define DBG_VERBOSE(...) DEBUG_SERIAL.printf(__VA_ARGS__)
#else
  // No-op macros when debugging is disabled
  #define Dbg_begin(...)
  #define Dbg_end(...)
  #define Dbg_print(...)
  #define Dbg_println(...)
  #define Dbg_printf(...)
  #define Dbg_write(...)
  #define DBG_VERBOSE(...)
#endif

#endif  // DBGPRINT