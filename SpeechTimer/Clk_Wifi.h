#ifndef CLK_WIFI_H
#define CLK_WIFI_H

#include <WiFi.h>
#include "config.h"

class Clk_Wifi {
public:
  // Constructor:
  Clk_Wifi(unsigned long wifi_update_interval = 1000);
  bool begin(Config *configuration);
  void checkConnection(int maxRetries = 1, bool fromBegin = false);
  IPAddress getIpAddress();
  bool hasIpAddress();
  bool isWiFiConnected();

  WiFiClient wifiClient;
  char ipAddress[16];
  WiFiMode_t wifiMode = WIFI_OFF;
  Config *config;
  WiFiMulti wifiMulti;
  int apAddedCount = 0;

private:
  int _beginLoopCount = 0;
  int _status = WL_IDLE_STATUS;
  unsigned long _wifi_check_previousMillis = 0;
  unsigned long _wifi_update_interval;
  bool _wifi_is_connected_last_state = false;
  const char *_logfile = "logs/wifi.log";
  long _wifi_connect_attempts = 0;
  unsigned long _connectionStartMillis = 0;

  void checkConnectionAccessPointMode(bool fromBegin = false);
  void checkConnectionStationMode(bool fromBegin = false);
};

#endif  // CLK_WIFI_H