#ifndef CLK_SDCARD_H
#define CLK_SDCARD_H

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "config.h"
#include "Defines.h"
#include "StructsAndEnums.h"
#include <map>
#include "DbgPrint.h"

class Clk_SdCard {
public:
  // Constructor: MISO, CS, SCK, MOSI, CD
  Clk_SdCard(pin_size_t miso = PIN_SDCARD_MISO, pin_size_t cs = PIN_SDCARD_CS, pin_size_t sck = PIN_SDCARD_SCK, pin_size_t mosi = PIN_SDCARD_MOSI, pin_size_t cd = PIN_SDCARD_CD);

  bool begin();

  void clearConfig();

  bool deleteFile(const char *fileFullName);

  bool isCardPresent();

  bool fileExists(const char *fileFullName);

  bool loadConfig(const char *fileFullName = "");

  void printConfig();

  File readFile(const char *fileFullName);

  bool renameFile(const char *fromFileFullName, const char *toFileFullName);

  bool writeLogEntry(const char *fileFullName = "clock.log", const char *message = "");

  bool cardPresent = false;
  char sdCardConfigFullName[128] = "config.txt";
  Config sdCardConfig = Config();

private:
  pin_size_t _miso, _cs, _sck, _mosi, _cd;
  std::map<DebugLevels, const char *> debugLevelName{ { DebugLevels::Verbose, "Verbose" }, { DebugLevels::Info, "Info" }, { DebugLevels::Warning, "Warning" }, { DebugLevels::Error, "Error" } };
};

#endif  // CLK_SDCARD_H