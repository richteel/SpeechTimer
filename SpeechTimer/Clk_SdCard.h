#ifndef CLK_SDCARD_H
#define CLK_SDCARD_H

#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <queue.h>
#include <semphr.h>
#include "config.h"
#include "Defines.h"
#include "StructsAndEnums.h"
#include <map>
#include "DbgPrint.h"

// SD Card reliability constants
#define LOG_BUFFER_SIZE 10                    // Buffer up to 10 log entries before writing
#define LOG_FLUSH_INTERVAL_MS 5000            // Flush buffer every 5 seconds
#define LOG_MAX_FILE_SIZE 102400              // Rotate logs when they exceed 100KB
#define SD_INIT_DEBOUNCE_MS 100               // Debounce time after card state change
#define SD_INIT_TIMEOUT_MS 10000              // Maximum time for SD initialization

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

  bool flushLogBuffer(const char *fileFullName = "clock.log");

  void setLogQueue(QueueHandle_t queue);

  bool queueLogEntry(const char *fileFullName = "clock.log", const char *message = "");

  bool beginIo(TickType_t timeoutTicks);

  void endIo();

  bool cardPresent = false;
  char sdCardConfigFullName[128] = "config.txt";
  Config sdCardConfig = Config();

private:
  pin_size_t _miso, _cs, _sck, _mosi, _cd;
  std::map<DebugLevels, const char *> debugLevelName{ { DebugLevels::Verbose, "Verbose" }, { DebugLevels::Info, "Info" }, { DebugLevels::Warning, "Warning" }, { DebugLevels::Error, "Error" } };
  
  // Log batching
  struct LogEntry {
    char message[256];
  };
  LogEntry _logBuffer[LOG_BUFFER_SIZE];
  int _logBufferCount = 0;
  unsigned long _lastLogFlush = 0;
  
  // Power loss protection
  bool _initInProgress = false;
  unsigned long _initStartTime = 0;
  
  // Debounce
  bool _lastCardState = false;
  unsigned long _lastCardStateChange = 0;

  // Serialize SD card access
  SemaphoreHandle_t _ioMutex = nullptr;

  // Log queue for async logging
  QueueHandle_t _logQueue = nullptr;

  bool takeIoMutex(TickType_t timeoutTicks);

  void giveIoMutex();
};

#endif  // CLK_SDCARD_H