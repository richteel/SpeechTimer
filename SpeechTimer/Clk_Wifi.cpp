#include "Clk_Wifi.h"
#include "Defines.h"  // For constants

#define FILE_NAME_WIFI "[Clk_Wifi.cpp]"

// ***** PRIVATE *****
// MIN_IP_ADDRESS_LEN is the minimum length of an IP address string (e.g. 1.1.1.1)
#define MIN_IP_ADDRESS_LEN 7

// ***** PUBLIC *****
Clk_Wifi::Clk_Wifi(unsigned long wifi_update_interval) {
  _wifi_update_interval = wifi_update_interval;
}

bool Clk_Wifi::begin(Config *configuration) {
  // Validate pointer parameter
  if (configuration == nullptr) {
    config = nullptr;
    return false;
  }
  
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
    DEBUGV("%s begin ------^^------ WIFI: Starting AP Mode ------^^------\n", FILE_NAME_WIFI);

    return true;
  } else {
    // Start Station Mode
    wifiMode = WIFI_STA;
    WiFi.mode(wifiMode);
    WiFi.setHostname(config->hostname);

    DEBUGV("%s begin - Connecting WiFi...\n", FILE_NAME_WIFI);
    unsigned long startConnect = millis();
    
    // Use iteration instead of recursion to prevent stack overflow
    constexpr uint8_t MAX_ATTEMPTS = MAX_WIFI_CONNECT_ATTEMPTS;
    uint8_t attempt = 0;
    bool connected = false;
    
    while (attempt < MAX_ATTEMPTS) {
      if (wifiMulti.run() == WL_CONNECTED) {
        connected = true;
        DEBUGV("%s begin - WiFi connected, SSID: %s\n", FILE_NAME_WIFI, WiFi.SSID().c_str());
        // Optimize: Avoid temporary String object by formatting IP directly
        IPAddress ip = WiFi.localIP();
        DEBUGV("%s begin - IP address: %d.%d.%d.%d\n", FILE_NAME_WIFI, ip[0], ip[1], ip[2], ip[3]);
        DEBUGV("%s begin - Connected in %d ms\n", FILE_NAME_WIFI, millis() - startConnect);
        break;
      }
      
      attempt++;
      if (attempt < MAX_ATTEMPTS) {
        DEBUGV("%s begin - Attempt %d/%d failed. Retrying...\n", FILE_NAME_WIFI, attempt, MAX_ATTEMPTS);
        delay(1000);
      }
    }
    
    if (!connected) {
      DEBUGV("%s begin - Failed to connect after %d attempts\n", FILE_NAME_WIFI, MAX_ATTEMPTS);
    }

    DEBUGV("%s begin ------^^------ WIFI: Starting Station Mode ------^^------\n", FILE_NAME_WIFI);

    return connected;
  }
}

void Clk_Wifi::checkConnection(int maxRetries, bool fromBegin) {
  if (wifiMode == WIFI_AP) {
    checkConnectionAccessPointMode(fromBegin);
    return;
  }
  
  if (wifiMode != WIFI_STA) {
    return;  // Only check station mode
  }
  
  checkConnectionStationMode(fromBegin);
}

IPAddress Clk_Wifi::getIpAddress() {
  return WiFi.localIP();
}

bool Clk_Wifi::hasIpAddress() {
  // Optimize: Avoid temporary String object by formatting IP directly
  IPAddress ip = WiFi.localIP();
  
  // Check if IP is valid (not 0.0.0.0)
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
    ipAddress[0] = '\0';
    return false;
  }
  
  // Format IP address directly without String allocation
  snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  
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
  // In AP mode, we're always "connected" - just verify the mode is active
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    DEBUGV("%s checkConnectionAccessPointMode - AP Mode active\n", FILE_NAME_WIFI);
  } else {
    DEBUGV("%s checkConnectionAccessPointMode - WARNING: AP Mode not active\n", FILE_NAME_WIFI);
  }
}

void Clk_Wifi::checkConnectionStationMode(bool fromBegin) {
  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    DEBUGV("%s checkConnectionStationMode - Already connected to %s\n", 
           FILE_NAME_WIFI, WiFi.SSID().c_str());
    return;
  }
  
  // Attempt to reconnect
  DEBUGV("%s checkConnectionStationMode - Connection lost, attempting to reconnect...\n", FILE_NAME_WIFI);
  
  if (wifiMulti.run() == WL_CONNECTED) {
    DEBUGV("%s checkConnectionStationMode - Reconnected to %s\n", 
           FILE_NAME_WIFI, WiFi.SSID().c_str());
    // Optimize: Avoid temporary String object by formatting IP directly
    IPAddress ip = WiFi.localIP();
    DEBUGV("%s checkConnectionStationMode - IP address: %d.%d.%d.%d\n", 
           FILE_NAME_WIFI, ip[0], ip[1], ip[2], ip[3]);
  } else {
    DEBUGV("%s checkConnectionStationMode - Failed to reconnect\n", FILE_NAME_WIFI);
  }
}