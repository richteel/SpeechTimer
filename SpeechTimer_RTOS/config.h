#ifndef CONFIG
#define CONFIG

#define NETWORKINFOARRAYSIZE 8

struct NetworkInfo {
  char ssid[256];
  char pass[256];
};

struct Config {
  char hostname[256];
  char timezone[256];
  NetworkInfo networks[NETWORKINFOARRAYSIZE];
};

#endif  // CONFIG