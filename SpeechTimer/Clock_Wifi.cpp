#include "Clock_Wifi.h"

// ***** PUBLIC *****
Clock_Wifi::Clock_Wifi(Clock_SdCard *sdcard, unsigned long wifi_update_interval) {
  _sdcard = sdcard;
  _wifi_update_interval = wifi_update_interval;
}

bool Clock_Wifi::begin() {
  _configLoaded = strlen(_sdcard->config.networks[0].ssid) > 0;

  if (!_configLoaded) {
    _configLoaded = _sdcard->loadConfig();
  }

  if (!_configLoaded) {
    // Start AP Mode
    WiFi.mode(WIFI_AP);
    _wifiMode = WIFI_AP;

    return true;
  }

  int i = 0;
  int added = 0;
  for (NetworkInfo netinfo : _sdcard->config.networks) {
    const char *ssid = netinfo.ssid;
    const char *pass = netinfo.pass;
    i++;

    // Add known WiFi networks
    if (strlen(ssid) > 0) {
      _WiFiMulti.addAP(ssid, pass);
      added++;
    }
  }

  if (added > 0) {
    WiFi.mode(WIFI_STA);
    _wifiMode = WIFI_STA;
    checkConnection(4);

    return true;
  }


  return false;
}

void Clock_Wifi::checkConnection(int maxRetries) {
  // Prevent updating the connection too often, especially if called back to back.
  if ((millis() - _wifi_check_previousMillis) >= _wifi_update_interval || millis() < _wifi_update_interval) {
    int retryCount = 0;
    bool successfulConnection = false;

    while (!successfulConnection && retryCount < maxRetries) {
      if (_wifiMode == WIFI_AP) {
        checkConnectionAccessPointMode();
        successfulConnection = true;
      } else if (_wifiMode == WIFI_STA) {
        checkConnectionStationMode();
        successfulConnection = isConnectedToInternet();
      }
      retryCount++;
    }

    _wifi_check_previousMillis = millis();
  }
}

char *Clock_Wifi::getIpAddress() {
  return ipAddress;
}

bool Clock_Wifi::isConnectedToInternet() {
  return (strlen(ipAddress) > 7) && (_wifiMode == WIFI_STA);
}

// ***** PRIVATE *****
void Clock_Wifi::checkConnectionAccessPointMode() {
}

void Clock_Wifi::checkConnectionStationMode() {
  unsigned long loopStartMillis = millis();
  bool activity = false;
  char buffer[256];
  bool wifi_is_connected = isConnectedToInternet();

  if (_wifiMode == WIFI_AP) {
    return;
  }

  // WiFi.status() != WL_CONNECTED does not seem to the be the best way
  // to determine if connected when Serial.begin() is not called
  // if (WiFi.status() != WL_CONNECTED) {
  if (!wifi_is_connected) {
    WiFi.disconnect();
    if (_wifi_is_connected_last_state) {
      _sdcard->writeLogEntry(_logfile, "--- Lost WiFi Connection ---", LOG_INFO);
      _wifi_is_connected_last_state = false;
      activity = true;
      _wifi_connect_attempts = 0;
      _connectionStartMillis = millis();
    }
    _WiFiMulti.run();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!_wifi_is_connected_last_state) {
      _sdcard->writeLogEntry(_logfile, "*** Connected to WiFI ***", LOG_INFO);
      sprintf(buffer, "               SSID:\t%s", WiFi.SSID().c_str());
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
      sprintf(buffer, "         IP Address:\t%s", WiFi.localIP().toString().c_str());
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
      sprintf(buffer, "            Gateway:\t%s", WiFi.gatewayIP().toString().c_str());
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
      strcpy(ipAddress, WiFi.localIP().toString().c_str());
      _wifi_is_connected_last_state = true;
      activity = true;
    }
  }

  if (activity) {
    if (_wifi_is_connected_last_state) {
      sprintf(buffer, "Connection Attempts:\t%d", (_wifi_connect_attempts + 1));
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
      sprintf(buffer, "    Connection Time:\t%d ms", (millis() - _connectionStartMillis));
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
      sprintf(buffer, "  Current Loop Time:\t%d ms", (millis() - loopStartMillis));
      _sdcard->writeLogEntry(_logfile, buffer, LOG_INFO);
    }
  }

  _wifi_connect_attempts++;
  _wifi_is_connected_last_state = wifi_is_connected;
}