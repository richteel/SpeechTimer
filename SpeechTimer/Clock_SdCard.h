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

#define LOG_INFO "INFO"
#define LOG_WARNING "WARNING"
#define LOG_ERROR "ERROR"


class Clock_SdCard {
public:
  // Constructor: MISO, CS, SCK, MOSI, CD
  Clock_SdCard(pin_size_t miso = 16, pin_size_t cs = 17, pin_size_t sck = 18, pin_size_t mosi = 19, pin_size_t cd = 20);

  bool begin();

  void clearConfig();

  uint countCharInFile(const char *fileFullName, char charToCount);

  uint countLinesInFile(const char *fileFullName);

  void end();

  bool fileExists(const char *fileFullName);

  bool isCardPresent();

  bool isInitialized();

  bool loadConfig(const char *fileFullName = "config.txt");

  File readFile(const char *fileFullName);

  bool saveConfig(const char *fileFullName);

  void writeLogEntry(const char *fileFullName = "clock.log", const char *message = "", const char *errorlevel = LOG_INFO);

  Config config = Config();

private:
  pin_size_t _miso, _cs, _sck, _mosi, _cd;
  bool _initialized = false;
};

#endif