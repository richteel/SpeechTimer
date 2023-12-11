#include "Clk_Wifi.h"

#define FILE_NAME_WIFI "[Clk_Wifi.cpp]"

// ***** PRIVATE *****
// MIN_IP_ADDRESS_LEN is the minimum length of an IP address string (e.g. 1.1.1.1)
#define MIN_IP_ADDRESS_LEN 7

// ***** PUBLIC *****
Clk_Wifi::Clk_Wifi(unsigned long wifi_update_interval) {
  _wifi_update_interval = wifi_update_interval;
}

bool Clk_Wifi::begin(Config *configuration) {
  config = configuration;
  apAddedCount = 0;

  bool wifiConfigLoaded = config->networks[0].ssid[0] != '\0';

  WiFi.disconnect();
  wifiMulti.clearAPList();

  if (wifiConfigLoaded) {
    // Add Access Points from Configuration
    DEBUGV("%s begin ------^^------ WIFI: Adding APs ------^^------\n", FILE_NAME_WIFI);
    for (NetworkInfo netinfo : config->networks) {
      const char *ssid = netinfo.ssid;
      const char *pass = netinfo.pass;

      // Add known WiFi networks
      if (ssid[0] != '\0') {
        if (wifiMulti.addAP(ssid, pass)) {
          apAddedCount++;
        }
        DEBUGV("%s begin ------^^------ WIFI: Added AP %s ------^^------\n", FILE_NAME_WIFI, ssid);
      }
    }
  }

  if (apAddedCount == 0) {
    // Start AP Mode
    wifiMode = WIFI_AP;
    WiFi.mode(wifiMode);
    checkConnection(4, true);
    DEBUGV("%s begin ------^^------ WIFI: Staring AP Mode ------^^------\n", FILE_NAME_WIFI);

    return true;
  } else {
    // Start Station Mode
    wifiMode = WIFI_STA;
    WiFi.mode(wifiMode);
    WiFi.setHostname(config->hostname);

    DEBUGV("%s begin - Connecting WiFi...\n", FILE_NAME_WIFI);
    unsigned long startConnect = millis();
    if (wifiMulti.run() != WL_CONNECTED) {
      if (_beginLoopCount < 5) {
        DEBUGV("%s begin - Failed to connect to Wi-Fi. Trying again...\n", FILE_NAME_WIFI);
        _beginLoopCount++;
        delay(1000);
        begin(configuration);
      } else {
        DEBUGV("%s begin - Failed to connect to Wi-Fi. Quiting\n", FILE_NAME_WIFI);
        _beginLoopCount = 0;
      }
    } else {
      _beginLoopCount = 0;
      DEBUGV("%s begin - WiFi connected, SSID: %s\n", FILE_NAME_WIFI, WiFi.SSID().c_str());
      DEBUGV("%s begin - IP address: %s\n", FILE_NAME_WIFI, WiFi.localIP().toString().c_str());
      DEBUGV("%s begin - Connected in %d ms\n", FILE_NAME_WIFI, millis() - startConnect);
    }

    // checkConnection(4, true);
    DEBUGV("%s begin ------^^------ WIFI: Starting Station Mode ------^^------\n", FILE_NAME_WIFI);

    return true;
  }
}

void Clk_Wifi::checkConnection(int maxRetries, bool fromBegin) {
}

IPAddress Clk_Wifi::getIpAddress() {
  return WiFi.localIP();
}

bool Clk_Wifi::hasIpAddress() {
  strcpy(ipAddress, WiFi.localIP().toString().c_str());
  if (strcmp(ipAddress, "(IP unset)") == 0) {
    ipAddress[0] = '\0';
  }

  return (strlen(ipAddress) > MIN_IP_ADDRESS_LEN) && (wifiMode == WIFI_STA);
}

bool isWiFiConnected() {
  // You can change longer or shorter depending on your network response
  // Shorter => more responsive, but more ping traffic
  static uint8_t theTTL = 10;

  // Use ping() to test TCP connections
  if (WiFi.ping(WiFi.gatewayIP(), theTTL) == theTTL) {
    return true;
  }

  return false;
}

// ***** PRIVATE *****
void Clk_Wifi::checkConnectionAccessPointMode(bool fromBegin) {
}

void Clk_Wifi::checkConnectionStationMode(bool fromBegin) {
}