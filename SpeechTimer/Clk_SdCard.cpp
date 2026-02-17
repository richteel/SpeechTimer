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
  if (_ioMutex == nullptr) {
    _ioMutex = xSemaphoreCreateRecursiveMutex();
  }

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
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return false;
  }

  if (!cardPresent) {
    giveIoMutex();
    return false;
  }

  if (!fileFullName || strlen(fileFullName) == 0) {
    giveIoMutex();
    return false;
  }

  if (fileExists(fileFullName)) {
    bool success = SD.remove(fileFullName);
    if (!success) {
      DBG_VERBOSE("%s deleteFile - ERROR: Failed to delete file: %s\n", FILE_NAME_CARD, fileFullName);
    }
    giveIoMutex();
    return success;
  }
  giveIoMutex();
  return false;
}

bool Clk_SdCard::isCardPresent() {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return cardPresent;
  }

  bool currentCardState;
  
  // If card detect pin is present, use it to determine if there is a sd card.
  if (_cd != NO_PIN) {
    currentCardState = digitalRead(_cd);
  } else {
    // Without card detect pin, assume card is still present if it was before
    // Only re-probe if we think the card is absent
    currentCardState = cardPresent;
  }
  
  // Check if state has changed
  if (currentCardState != _lastCardState) {
    _lastCardStateChange = millis();
    _lastCardState = currentCardState;
  }
  
  // Only process state change after debounce period
  unsigned long timeSinceChange = millis() - _lastCardStateChange;
  if (timeSinceChange < SD_INIT_DEBOUNCE_MS) {
    // Still debouncing, return previous state
    giveIoMutex();
    return cardPresent;
  }
  
  // Check for initialization timeout (power loss protection)
  if (_initInProgress) {
    unsigned long timeSinceInit = millis() - _initStartTime;
    if (timeSinceInit > SD_INIT_TIMEOUT_MS) {
      DBG_VERBOSE("%s isCardPresent - WARNING: SD init timeout (possible power loss), aborting\n", FILE_NAME_CARD);
      _initInProgress = false;
      cardPresent = false;
      SD.end();
      giveIoMutex();
      return cardPresent;
    }
    // Still initializing, return current state
    giveIoMutex();
    return cardPresent;
  }
  
  // Only call SD.begin() if transitioning from absent to present
  if (!cardPresent && currentCardState) {
    _initInProgress = true;
    _initStartTime = millis();
    DBG_VERBOSE("%s isCardPresent - Card inserted, initializing...\n", FILE_NAME_CARD);
    cardPresent = SD.begin(_cs);
    _initInProgress = false;
    
    if (cardPresent) {
      DBG_VERBOSE("%s isCardPresent - Card initialized successfully, loading config\n", FILE_NAME_CARD);
      loadConfig();
    }
  }
  // Only call SD.end() if transitioning from present to absent
  else if (cardPresent && !currentCardState) {
    DBG_VERBOSE("%s isCardPresent - Card removed\n", FILE_NAME_CARD);
    flushLogBuffer();  // Flush any pending logs before shutdown
    SD.end();
    cardPresent = false;
  }

  giveIoMutex();
  return cardPresent;
}

bool Clk_SdCard::fileExists(const char *fileFullName) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return false;
  }

  if (!cardPresent) {
    giveIoMutex();
    return false;
  }

  if (!fileFullName || strlen(fileFullName) == 0) {
    giveIoMutex();
    return false;
  }

  bool exists = SD.exists(fileFullName);
  giveIoMutex();
  return exists;
}

bool Clk_SdCard::loadConfig(const char *fileFullName) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_MS))) {
    return false;
  }

  if (strlen(fileFullName) > 0) {
    strlcpy(sdCardConfigFullName, fileFullName, sizeof(sdCardConfigFullName));
  }

  File f = readFile(sdCardConfigFullName);

  if (!f) {
    DBG_VERBOSE("%s loadConfig - ERROR: Failed to open config file: %s\n", FILE_NAME_CARD, sdCardConfigFullName);
    giveIoMutex();
    return false;
  }

  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, f);
  if (error) {
    DBG_VERBOSE("%s loadConfig - ERROR: JSON parse failed: %s\n", FILE_NAME_CARD, error.c_str());
    f.close();
    giveIoMutex();
    return false;
  }

  // Clear existing config
  clearConfig();
  
  // Load hostname with truncation warning
  const char* hostnameValue = doc["hostname"] | "speechtimer";
  size_t hostnameLen = strlen(hostnameValue);
  strlcpy(sdCardConfig.hostname, hostnameValue, sizeof(sdCardConfig.hostname));
  if (hostnameLen >= sizeof(sdCardConfig.hostname)) {
    DBG_VERBOSE("%s loadConfig - WARNING: hostname truncated from %d to %d chars\n", 
           FILE_NAME_CARD, hostnameLen, sizeof(sdCardConfig.hostname) - 1);
  }
  
  // Load timezone with truncation warning
  const char* timezoneValue = doc["timezone"] | "";
  size_t timezoneLen = strlen(timezoneValue);
  strlcpy(sdCardConfig.timezone, timezoneValue, sizeof(sdCardConfig.timezone));
  if (timezoneLen >= sizeof(sdCardConfig.timezone)) {
    DBG_VERBOSE("%s loadConfig - WARNING: timezone truncated from %d to %d chars\n", 
           FILE_NAME_CARD, timezoneLen, sizeof(sdCardConfig.timezone) - 1);
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
        DBG_VERBOSE("%s loadConfig - WARNING: network[%d] SSID truncated from %d to %d chars\n", 
               FILE_NAME_CARD, i, ssidLen, sizeof(sdCardConfig.networks[i].ssid) - 1);
      }
      
      // Load password with truncation warning
      const char* passValue = repo["pass"];
      size_t passLen = strlen(passValue);
      strlcpy(sdCardConfig.networks[i].pass, passValue, sizeof(sdCardConfig.networks[i].pass));
      if (passLen >= sizeof(sdCardConfig.networks[i].pass)) {
        DBG_VERBOSE("%s loadConfig - WARNING: network[%d] password truncated from %d to %d chars\n", 
               FILE_NAME_CARD, i, passLen, sizeof(sdCardConfig.networks[i].pass) - 1);
      }
      
      DBG_VERBOSE("%s loadConfig - %d.\t%s\t%s\n", FILE_NAME_CARD, i, sdCardConfig.networks[i].ssid, sdCardConfig.networks[i].pass);
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
      DBG_VERBOSE("%s loadConfig - %d.\t%d\t%d\t%s\n", FILE_NAME_CARD, j, sdCardConfig.timers[j].min, sdCardConfig.timers[j].max, sdCardConfig.timers[j].manual ? "true" : "false");
    }
    j++;
  }

  f.close();

  giveIoMutex();
  return true;
}

void Clk_SdCard::printConfig() {
  Config *config = &sdCardConfig;
  int cnt = 0;

  DBG_VERBOSE("%s printConfig - Hostname = %s\n", FILE_NAME_CARD, config->hostname);

  for (NetworkInfo netinfo : config->networks) {
    const char *ssid = netinfo.ssid;
    const char *pass = netinfo.pass;

    // Add known WiFi networks
    DBG_VERBOSE("%s printConfig - %d\t%s\t%s\n", FILE_NAME_CARD, cnt, ssid, pass);
    cnt++;
  }
}

File Clk_SdCard::readFile(const char *fileFullName) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return FileImplPtr();
  }

  if (!cardPresent) {
    giveIoMutex();
    return FileImplPtr();
  }

  if (!fileFullName || strlen(fileFullName) == 0) {
    DBG_VERBOSE("%s readFile - ERROR: Invalid filename\n", FILE_NAME_CARD);
    giveIoMutex();
    return FileImplPtr();
  }

  File f = SD.open(fileFullName, FILE_READ);
  if (!f) {
    DBG_VERBOSE("%s readFile - ERROR: Failed to open file: %s\n", FILE_NAME_CARD, fileFullName);
  }
  giveIoMutex();
  return f;
}

bool Clk_SdCard::renameFile(const char *fromFileFullName, const char *toFileFullName) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return false;
  }

  if (!cardPresent) {
    giveIoMutex();
    return false;
  }

  if (!fromFileFullName || strlen(fromFileFullName) == 0 || !toFileFullName || strlen(toFileFullName) == 0) {
    giveIoMutex();
    return false;
  }

  if (fileExists(toFileFullName)) {
    deleteFile(toFileFullName);
  }

  if (fileExists(fromFileFullName)) {
    bool success = SD.rename(fromFileFullName, toFileFullName);
    if (!success) {
      DBG_VERBOSE("%s renameFile - ERROR: Failed to rename %s to %s\n", FILE_NAME_CARD, fromFileFullName, toFileFullName);
    }
    giveIoMutex();
    return success;
  }

  giveIoMutex();
  return false;
}

bool Clk_SdCard::writeLogEntry(const char *fileFullName, const char *message) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS))) {
    return false;
  }

  // Early exit if no card present
  if (!cardPresent) {
    giveIoMutex();
    return false;
  }

  // Validate input
  if (!fileFullName || strlen(fileFullName) == 0 || !message) {
    giveIoMutex();
    return false;
  }

  // Add message to buffer
  if (_logBufferCount < LOG_BUFFER_SIZE) {
    strlcpy(_logBuffer[_logBufferCount].message, message, sizeof(_logBuffer[_logBufferCount].message));
    _logBufferCount++;
  } else {
    // Buffer full, flush it
    flushLogBuffer(fileFullName);
    // Try adding again
    if (_logBufferCount < LOG_BUFFER_SIZE) {
      strlcpy(_logBuffer[_logBufferCount].message, message, sizeof(_logBuffer[_logBufferCount].message));
      _logBufferCount++;
    } else {
      DBG_VERBOSE("%s writeLogEntry - ERROR: Log buffer overflow\n", FILE_NAME_CARD);
      giveIoMutex();
      return false;
    }
  }

  // Check if it's time to flush (5 second interval)
  unsigned long now = millis();
  if (now - _lastLogFlush > LOG_FLUSH_INTERVAL_MS) {
    flushLogBuffer(fileFullName);
  }

  giveIoMutex();
  return true;
}

bool Clk_SdCard::flushLogBuffer(const char *fileFullName) {
  if (!takeIoMutex(pdMS_TO_TICKS(MUTEX_TIMEOUT_MS))) {
    return false;
  }

  // Early exit if no entries to flush or no card
  if (_logBufferCount == 0 || !cardPresent) {
    _lastLogFlush = millis();
    giveIoMutex();
    return true;  // Nothing to flush is not an error
  }

  // Validate input
  if (!fileFullName || strlen(fileFullName) == 0) {
    giveIoMutex();
    return false;
  }

  // Check if file exists and get size for rotation
  uint32_t fileSize = 0;
  if (SD.exists(fileFullName)) {
    File checkFile = SD.open(fileFullName, FILE_READ);
    if (checkFile) {
      fileSize = checkFile.size();
      checkFile.close();
    }
  }

  // Rotate log if it's too large (before opening for write)
  if (fileSize > LOG_MAX_FILE_SIZE) {
    char backupName[128];
    snprintf(backupName, sizeof(backupName), "%s.bak", fileFullName);
    
    // Delete old backup if it exists
    if (SD.exists(backupName)) {
      SD.remove(backupName);
    }
    
    // Rename current log to backup
    if (SD.exists(fileFullName)) {
      bool success = SD.rename(fileFullName, backupName);
      if (success) {
        DBG_VERBOSE("%s flushLogBuffer - Log rotated: %s -> %s\n", FILE_NAME_CARD, fileFullName, backupName);
      } else {
        DBG_VERBOSE("%s flushLogBuffer - WARNING: Failed to rotate log file\n", FILE_NAME_CARD);
      }
    }
  }

  // Open file for appending
  File logFile = SD.open(fileFullName, FILE_WRITE);
  
  // Validate file opened successfully
  if (!logFile) {
    DBG_VERBOSE("%s flushLogBuffer - ERROR: Failed to open log file: %s\n", FILE_NAME_CARD, fileFullName);
    giveIoMutex();
    return false;
  }

  // Seek to end of file to ensure append behavior
  logFile.seek(logFile.size());
  
  // Write all buffered entries
  size_t totalBytesWritten = 0;
  for (int i = 0; i < _logBufferCount; i++) {
    size_t bytesWritten = logFile.println(_logBuffer[i].message);
    totalBytesWritten += bytesWritten;
    
    if (bytesWritten == 0) {
      DBG_VERBOSE("%s flushLogBuffer - WARNING: Failed to write entry %d\n", FILE_NAME_CARD, i);
    }
  }
  
  // Explicitly flush to ensure data is written to card
  logFile.flush();
  
  // Close the file
  logFile.close();

  // Check if write was successful
  if (totalBytesWritten == 0) {
    DBG_VERBOSE("%s flushLogBuffer - WARNING: No bytes written to log file: %s\n", FILE_NAME_CARD, fileFullName);
    giveIoMutex();
    return false;
  }

  // Clear buffer after successful flush
  _logBufferCount = 0;
  _lastLogFlush = millis();
  
  giveIoMutex();
  return true;
}

void Clk_SdCard::setLogQueue(QueueHandle_t queue) {
  _logQueue = queue;
}

bool Clk_SdCard::queueLogEntry(const char *fileFullName, const char *message) {
  if (_logQueue == nullptr || !fileFullName || strlen(fileFullName) == 0 || !message) {
    return false;
  }

  Log_Entry logEntry;
  strlcpy(logEntry.message, message, sizeof(logEntry.message));
  strlcpy(logEntry.logfile, fileFullName, sizeof(logEntry.logfile));

  if (xQueueSend(_logQueue, (void *)&logEntry, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) == pdTRUE) {
    return true;
  }

  // Queue is full - try to make room by dropping oldest entry
  Log_Entry dummy;
  if (xQueueReceive(_logQueue, &dummy, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
    if (xQueueSend(_logQueue, (void *)&logEntry, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
      return true;
    }
  }

  Dbg_printf("%s: %s queueLogEntry - Log Queue Full, entry dropped\n",
             debugLevelName[DebugLevels::Warning], FILE_NAME_CARD);
  return false;
}

bool Clk_SdCard::beginIo(TickType_t timeoutTicks) {
  return takeIoMutex(timeoutTicks);
}

void Clk_SdCard::endIo() {
  giveIoMutex();
}

bool Clk_SdCard::takeIoMutex(TickType_t timeoutTicks) {
  if (_ioMutex == nullptr) {
    return true;
  }
  return xSemaphoreTakeRecursive(_ioMutex, timeoutTicks) == pdTRUE;
}

void Clk_SdCard::giveIoMutex() {
  if (_ioMutex != nullptr) {
    xSemaphoreGiveRecursive(_ioMutex);
  }
}
