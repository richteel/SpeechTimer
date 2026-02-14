#ifndef CONFIG
#define CONFIG

#define NETWORKINFOARRAYSIZE 8

struct NetworkInfo {
  char ssid[32];   // Reduced from 256 to 32 (typical WiFi SSID max length)
  char pass[64];   // Reduced from 256 to 64 (adequate for most passwords)
};

#define TIMERARRAYSIZE 4

struct TimerInfo {
  int min;
  int max;
  bool manual;
};

struct Config {
  char hostname[32];  // Reduced from 256 to 32 (typical hostname length)
  char timezone[64];  // Reduced from 256 to 64 (sufficient for timezone strings)
  NetworkInfo networks[NETWORKINFOARRAYSIZE];
  TimerInfo timers[TIMERARRAYSIZE];
};

#endif  // CONFIG