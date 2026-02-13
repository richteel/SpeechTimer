#include "Clk_SdCard.h"

#define FILE_NAME_CARD "[Clk_SdCard.cpp]"

// Constructor: MISO, CS, SCK, MOSI, CD
Clk_SdCard::Clk_SdCard(pin_size_t miso, pin_size_t cs, pin_size_t sck, pin_size_t mosi, pin_size_t cd) {
  _miso = miso;
  _cs = cs;
  _sck = sck;
  _mosi = mosi;
  _cd = cd;
}

bool Clk_SdCard::begin() {
  // Ensure the SPI pinout the SD card is connected to is configured properly
  SPI.setRX(_miso);
  SPI.setTX(_mosi);
  SPI.setSCK(_sck);

  // If there is a Card Detect Pin, set the cardPresent flag based on the value of the pin
  if (_cd != NO_PIN) {
    pinMode(_cd, INPUT);
  }

  // Call isCardPresent to do the following
  //  - Set the cardPresent flag
  //  - Call SD.begin
  //  - Load the configuration file if an SD Card is present
  isCardPresent();

  return cardPresent;
}

void Clk_SdCard::clearConfig() {
  sdCardConfig.hostname[0] = '\0';
  sdCardConfig.timezone[0] = '\0';

  for (NetworkInfo& ni : sdCardConfig.networks) {  // Fixed: Use reference, not copy
    ni.ssid[0] = '\0';
    ni.pass[0] = '\0';
  }

  for (TimerInfo& ti : sdCardConfig.timers) {  // Fixed: Use reference, not copy
    ti.min = 0;
    ti.max = 0;
    ti.manual = true;
  }
}

bool Clk_SdCard::deleteFile(const char *fileFullName) {
  if (!cardPresent) {
    return false;
  }

  if (fileExists(fileFullName)) {
    SD.remove(fileFullName);
    return true;
  }
  return false;
}

bool Clk_SdCard::isCardPresent() {
  bool last_cardPresent = cardPresent;
  cardPresent = true;

  // If card detect pin is present, use it to determine if there is a sd card.
  if (_cd != NO_PIN) {
    cardPresent = digitalRead(_cd);
    // If there is no card, call SD.end.
    if (!cardPresent) {
      SD.end();
    }
  } else {
    SD.end();
    cardPresent = SD.begin(_cs);
  }

  // Attempt to reload the config file if the card was just inserted
  if (cardPresent && !last_cardPresent) {
    SD.begin(_cs);
    DEBUGV("%s isCardPresent - START: Reading Config File\n", FILE_NAME_CARD);
    loadConfig();
    DEBUGV("%s isCardPresent - FINISHED: Reading Config File\n", FILE_NAME_CARD);
  }

  return cardPresent;
}

bool Clk_SdCard::fileExists(const char *fileFullName) {
  if (!cardPresent) {
    return false;
  }

  bool fileExists = SD.exists(fileFullName);
  return fileExists;
}

bool Clk_SdCard::loadConfig(const char *fileFullName) {
  if (strlen(fileFullName) > 0) {
    strlcpy(sdCardConfigFullName, fileFullName, sizeof(sdCardConfigFullName));
  }

  File f = readFile(sdCardConfigFullName);

  if (!f) {
    DEBUGV("%s loadConfig - return false 1 sdCardConfigFullName: %s\n", FILE_NAME_CARD, sdCardConfigFullName);
    return false;
  }

  const int config_json_capacity = JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(NETWORKINFOARRAYSIZE) + NETWORKINFOARRAYSIZE * JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(TIMERARRAYSIZE) + TIMERARRAYSIZE * JSON_OBJECT_SIZE(2);

  StaticJsonDocument<config_json_capacity> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, f);
  if (error) {
    DEBUGV("%s loadConfig - return false 2 ERROR: %s\n", FILE_NAME_CARD, error);
    return false;
  } else {
    clearConfig();
    
    // Load hostname with truncation warning
    const char* hostnameValue = doc["hostname"] | "speechtimer";
    size_t hostnameLen = strlen(hostnameValue);
    strlcpy(sdCardConfig.hostname, hostnameValue, sizeof(sdCardConfig.hostname));
    if (hostnameLen >= sizeof(sdCardConfig.hostname)) {
      DEBUGV("%s loadConfig - WARNING: hostname truncated from %d to %d chars\n", 
             FILE_NAME_CARD, hostnameLen, sizeof(sdCardConfig.hostname) - 1);
      // TODO: Add visual indication on display/neopixels for config truncation warning
    }
    
    // Load timezone with truncation warning
    const char* timezoneValue = doc["timezone"] | "";
    size_t timezoneLen = strlen(timezoneValue);
    strlcpy(sdCardConfig.timezone, timezoneValue, sizeof(sdCardConfig.timezone));
    if (timezoneLen >= sizeof(sdCardConfig.timezone)) {
      DEBUGV("%s loadConfig - WARNING: timezone truncated from %d to %d chars\n", 
             FILE_NAME_CARD, timezoneLen, sizeof(sdCardConfig.timezone) - 1);
      // TODO: Add visual indication on display/neopixels for config truncation warning
    }

    JsonArray arrNetworks = doc["networks"].as<JsonArray>();
    int i = 0;
    for (JsonObject repo : arrNetworks) {
      if (i < NETWORKINFOARRAYSIZE) {
        // Load SSID with truncation warning
        const char* ssidValue = repo["ssid"];
        size_t ssidLen = strlen(ssidValue);
        strlcpy(sdCardConfig.networks[i].ssid, ssidValue, sizeof(sdCardConfig.networks[i].ssid));
        if (ssidLen >= sizeof(sdCardConfig.networks[i].ssid)) {
          DEBUGV("%s loadConfig - WARNING: network[%d] SSID truncated from %d to %d chars\n", 
                 FILE_NAME_CARD, i, ssidLen, sizeof(sdCardConfig.networks[i].ssid) - 1);
          // TODO: Add visual indication on display/neopixels for config truncation warning
        }
        
        // Load password with truncation warning
        const char* passValue = repo["pass"];
        size_t passLen = strlen(passValue);
        strlcpy(sdCardConfig.networks[i].pass, passValue, sizeof(sdCardConfig.networks[i].pass));
        if (passLen >= sizeof(sdCardConfig.networks[i].pass)) {
          DEBUGV("%s loadConfig - WARNING: network[%d] password truncated from %d to %d chars\n", 
                 FILE_NAME_CARD, i, passLen, sizeof(sdCardConfig.networks[i].pass) - 1);
          // TODO: Add visual indication on display/neopixels for config truncation warning
        }
        
        DEBUGV("%s loadConfig - %d.\t%s\t%s\n", FILE_NAME_CARD, i, sdCardConfig.networks[i].ssid, sdCardConfig.networks[i].pass);
      }
      i++;
    }

    JsonArray arrTimers = doc["timers"].as<JsonArray>();
    int j = 0;
    for (JsonObject repo : arrTimers) {
      if (j < TIMERARRAYSIZE) {
        sdCardConfig.timers[j].min = repo["min"];
        sdCardConfig.timers[j].max = repo["max"];
        sdCardConfig.timers[j].manual = repo["manual"];
        DEBUGV("%s loadConfig - %d.\t%d\t%d\t%s\n", FILE_NAME_CARD, j, sdCardConfig.timers[j].min, sdCardConfig.timers[j].max, sdCardConfig.timers[j].manual ? "true" : "false");
      }
      j++;
    }
  }

  f.close();

  return true;
}

void Clk_SdCard::printConfig() {
  Config *config = &sdCardConfig;
  int cnt = 0;

  DEBUGV("%s printConfig - Hostname = %s\n", FILE_NAME_CARD, config->hostname);

  for (NetworkInfo netinfo : config->networks) {
    const char *ssid = netinfo.ssid;
    const char *pass = netinfo.pass;

    // Add known WiFi networks
    DEBUGV("%s printConfig - %d\t%s\t%s\n", FILE_NAME_CARD, cnt, ssid, pass);
    cnt++;
  }
}

File Clk_SdCard::readFile(const char *fileFullName) {
  if (!cardPresent) {
    return FileImplPtr();
  }

  return SD.open(fileFullName, FILE_READ);
}

bool Clk_SdCard::renameFile(const char *fromFileFullName, const char *toFileFullName) {
  if (!cardPresent) {
    return false;
  }

  if (fileExists(toFileFullName)) {
    deleteFile(toFileFullName);
  }

  if (fileExists(fromFileFullName)) {
    SD.rename(fromFileFullName, toFileFullName);
    return true;
  }

  return false;
}

bool Clk_SdCard::writeLogEntry(const char *fileFullName, const char *message) {
  if (!cardPresent) {
    return false;
  }

  File logFile = SD.open(fileFullName, FILE_WRITE);
  logFile.println(message);
  logFile.close();

  return true;
}