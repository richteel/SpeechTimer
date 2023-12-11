#ifndef CONFIG
#define CONFIG

#define NETWORKINFOARRAYSIZE 8

struct NetworkInfo {
  char ssid[256];
  char pass[256];
};

#define TIMERARRAYSIZE 4

struct TimerInfo {
  int min;
  int max;
  bool manual;
};

struct Config {
  char hostname[256];
  char timezone[256];
  NetworkInfo networks[NETWORKINFOARRAYSIZE];
  TimerInfo timers[TIMERARRAYSIZE];
};

#endif  // CONFIG