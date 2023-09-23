#ifndef CLOCK_WIFI_H
#define CLOCK_WIFI_H

#include <WiFi.h>
#include "config.h"
#include "Clock_SdCard.h"

class Clock_Wifi {
public:
  // Constructor:
  Clock_Wifi(Clock_SdCard *sdcard, unsigned long wifi_update_interval = 1000);

  bool begin();

  void checkConnection(int maxRetries = 1);

  bool isConnectedToInternet();

  WiFiClient wifiClient;

private:
  Clock_SdCard *_sdcard;
  int _status = WL_IDLE_STATUS;
  bool _configLoaded = false;
  WiFiMulti _WiFiMulti;
  int _wifiMode = 0;
  unsigned long _wifi_check_previousMillis = 0;
  unsigned long _wifi_update_interval;
  bool _wifi_is_connected = true;  // Set to true so it will reset and trigger code to connect.
  const char *_logfile = "logs/wifi.log";
  long _wifi_connect_attempts = 0;
  unsigned long _connectionStartMillis = 0;

  void checkConnectionAccessPointMode();
  void checkConnectionStationMode();
};

#endif