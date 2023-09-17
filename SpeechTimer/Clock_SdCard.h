#ifndef CLOCK_SDCARD_H
#define CLOCK_SDCARD_H

#ifndef NO_PIN
#define NO_PIN 255
#endif

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "config.h"
#include "DebugPrint.h"

class Clock_SdCard {
public:
  // Constructor: MISO, CS, SCK, MOSI, CD
  Clock_SdCard(pin_size_t miso=16, pin_size_t cs=17, pin_size_t sck=18, pin_size_t mosi = 19, pin_size_t cd=20);

  bool begin();

  void clearConfig();

  void end();

  bool fileExists(const char *fileFullName);

  bool isCardPresent();

  bool isInitialized();

  bool loadConfig(const char *fileFullName);

  File readFile(const char *fileFullName);

  bool saveConfig(const char *fileFullName);

  Config config = Config();

private:
  pin_size_t _miso, _cs, _sck, _mosi, _cd;
  spi_inst_t *_spi;
  bool _initialized = false;
};

#endif