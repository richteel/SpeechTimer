#include "Clock_SdCard.h"

Clock_SdCard::Clock_SdCard(pin_size_t miso, pin_size_t cs, pin_size_t sck, pin_size_t mosi, pin_size_t cd) {
  _miso = miso;
  _cs = cs;
  _sck = sck;
  _mosi = mosi;
  _cd = cd;
}

bool Clock_SdCard::begin() {
  // Ensure the SPI pinout the SD card is connected to is configured properly
  SPI.setRX(_miso);
  SPI.setTX(_mosi);
  SPI.setSCK(_sck);

  if (_cd != NO_PIN) {
    pinMode(_cd, INPUT);
  }

  if (_cd == NO_PIN || digitalRead(_cd)) {
    _initialized = SD.begin(_cs);
  }

  return _initialized;
}

void Clock_SdCard::clearConfig() {
  config.hostname[0] = '\0';
  config.timezone[0] = '\0';

  for (NetworkInfo ni : config.networks) {
    ni.ssid[0] = '\0';
    ni.pass[0] = '\0';
  }
}

void Clock_SdCard::end() {
  SD.end();
}

bool Clock_SdCard::fileExists(const char *fileFullName) {
  bool cardPresent = isCardPresent();

  if (!cardPresent) {
    return false;
  }

  return SD.exists(fileFullName);
}

bool Clock_SdCard::isCardPresent() {
  bool cardPresent = true;

  // If card detect pin is present, use it to determine if there is a sd card.
  if (_cd != NO_PIN) {
    cardPresent = digitalRead(_cd);
    // If there is no card but the SD card was initialized, call end.
    // Else if there is a card and the SD card was not initialized, call begin
    if (!cardPresent && _initialized) {
      SD.end();
      _initialized = false;
      return false;
    }
  }

  SD.end();
  _initialized = SD.begin(_cs);
  return _initialized && cardPresent;
}

bool Clock_SdCard::isInitialized() {
  return _initialized;
}

bool Clock_SdCard::loadConfig(const char *fileFullName) {
  File f = readFile(fileFullName);

  if (!f) {
    return false;
  }

  const int config_json_capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(NETWORKINFOARRAYSIZE) + NETWORKINFOARRAYSIZE * JSON_OBJECT_SIZE(2);

  StaticJsonDocument<config_json_capacity> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, f);
  if (error) {
    return false;
  } else {
    clearConfig();
    strlcpy(config.hostname, doc["hostname"] | "speechtimer", sizeof(config.hostname));
    strlcpy(config.timezone, doc["timezone"] | "", sizeof(config.timezone));

    JsonArray arr = doc["networks"].as<JsonArray>();

    int i = 0;
    for (JsonObject repo : arr) {
      if (i < NETWORKINFOARRAYSIZE) {
        strlcpy(config.networks[i].ssid, repo["ssid"], sizeof(config.networks[i].ssid));
        strlcpy(config.networks[i].pass, repo["pass"], sizeof(config.networks[i].pass));
      }
      i++;
    }
  }

  f.close();

  return true;
}

File Clock_SdCard::readFile(const char *fileFullName) {
  if (!isCardPresent()) {
    return FileImplPtr();
  }

  return SD.open(fileFullName, FILE_READ);
}

bool Clock_SdCard::saveConfig(const char *fileFullName) {
  if (fileExists(fileFullName)) {
    SD.remove(fileFullName);
  }

  // Open file for writing
  File file = SD.open(fileFullName, FILE_WRITE);
  if (!file) {
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  const int config_json_capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(NETWORKINFOARRAYSIZE) + NETWORKINFOARRAYSIZE * JSON_OBJECT_SIZE(2);
  StaticJsonDocument<config_json_capacity> doc;

  doc["hostname"] = config.hostname;
  doc["timezone"] = config.timezone;
  JsonArray networks = doc.createNestedArray("networks");

  for (NetworkInfo ni : config.networks) {
    if (strlen(ni.ssid) > 0) {
      JsonObject nobj = networks.createNestedObject();
      nobj["ssid"] = ni.ssid;
      nobj["pass"] = ni.pass;
    }
  }

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0) {
    // Close the file
    file.close();
    return false;
  }

  // Close the file
  file.close();

  return true;
}