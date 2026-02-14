#include "Clk_Rtc.h"
#include "StructsAndEnums.h"
#include "Defines.h"  // For constants

// ***** PUBLIC *****
Clk_Rtc::Clk_Rtc(Clk_SdCard *sdcard, Clk_Wifi *clock_Wifi, unsigned long internet_update_interval) {
  // Validate pointer parameters
  if (sdcard == nullptr || clock_Wifi == nullptr) {
    _sdcard = nullptr;
    _clock_Wifi = nullptr;
    _internet_update_interval = internet_update_interval;
    return;
  }
  
  _sdcard = sdcard;
  _clock_Wifi = clock_Wifi;
  _internet_update_interval = internet_update_interval;
}

bool Clk_Rtc::begin() {
  setSyncInterval(14400);              // Update the time every 4 hours.
                                       // After 4 hours, calling timeStatus() will return timeNeedsSync.
  setTime(SECONDS_FROM_1970_TO_2000);  // Sat Jan 01 2000 00:00:00 GMT+0000

  return true;
}

long Clk_Rtc::getInternetTime(long timeoutMs) {
  long unixtime = 0;
  unsigned long startConnection;
  const char server[] = "worldtimeapi.org";
  bool receivedResponse = false;

  _last_update_successful = false;

  if (!_clock_Wifi->hasIpAddress()) {
    return unixtime;
  }

  // Add connection timeout to prevent hanging
  unsigned long connectStart = millis();
  bool connected = false;
  
  // Try to connect with timeout (use half the total timeout for connection)
  while (!connected && (millis() - connectStart <= timeoutMs / 2)) {
    if (_clock_Wifi->wifiClient.connect(server, 80)) {
      connected = true;
      break;
    }
    delay(HTTP_CONNECTION_RETRY_DELAY_MS);  // Brief delay before retry
  }
  
  if (!connected) {
    DEBUGV("[CLK_RTC] Connection timeout - failed to connect to %s\n", server);
    return unixtime;
  }

  // Send HTTP request
  if (strlen(_sdcard->sdCardConfig.timezone) == 0) {
    _clock_Wifi->wifiClient.println(F("GET /api/ip/? HTTP/1.1"));
    sprintf(timeUpdateUrl, "http://%s/api/ip/?", server);
  } else {
    _clock_Wifi->wifiClient.printf("GET /api/timezone/%s/? HTTP/1.1\n", _sdcard->sdCardConfig.timezone);
    sprintf(timeUpdateUrl, "http://%s/api/timezone/%s/?", server, _sdcard->sdCardConfig.timezone);
  }
  _clock_Wifi->wifiClient.println(F("Host: worldtimeapi.org"));
  _clock_Wifi->wifiClient.println(F("User-Agent: Lynx/2.8.9dev libwww-FM/2.14 SSL-MM/1.4.1 GNUTLS/3.3.8 (Raspberry Pi Pico W)"));
  _clock_Wifi->wifiClient.println(F("Accept: text/html,application/json"));
  _clock_Wifi->wifiClient.println(F("Connection: close"));
  _clock_Wifi->wifiClient.println();

  // Calculate remaining timeout after connection attempt
  unsigned long remainingTimeout = timeoutMs - (millis() - connectStart);
  if (remainingTimeout <= 0) {
    DEBUGV("[CLK_RTC] Timeout exhausted during connection\n");
    _clock_Wifi->wifiClient.stop();
    return unixtime;
  }

  startConnection = millis();
  bool foundResponseBody = false;
  char lastchars[5] = "\0\0\0\0";

  while (!receivedResponse && (millis() - startConnection <= remainingTimeout)) {
    while (_clock_Wifi->wifiClient.available()) {
      if (!foundResponseBody) {
        char readChar = _clock_Wifi->wifiClient.read();

        // Handle line endings of /r/n, /n/r, /n, & /r
        // shuffle lastchars array
        lastchars[0] = lastchars[1];
        lastchars[1] = lastchars[2];
        lastchars[2] = lastchars[3];
        lastchars[3] = readChar;

        // Handle one char line ending
        if ((lastchars[2] == '\r' || lastchars[2] == '\n') && (lastchars[2] == lastchars[3])) {
          foundResponseBody = true;
        } else if ((lastchars[0] == '\r' || lastchars[0] == '\n') && (lastchars[1] == '\r' || lastchars[1] == '\n') && (lastchars[2] == '\r' || lastchars[2] == '\n') && (lastchars[3] == '\r' || lastchars[3] == '\n')) {
          foundResponseBody = true;
        }
      } else {
        // Bounds checking: limit response buffer to prevent stack overflow
        constexpr size_t MAX_RESPONSE_SIZE = HTTP_RESPONSE_BUFFER_SIZE;
        int charCount = _clock_Wifi->wifiClient.available();
        if (charCount > MAX_RESPONSE_SIZE) {
          DEBUGV("[CLK_RTC] Warning: Response size %d exceeds maximum %d, truncating\n", charCount, MAX_RESPONSE_SIZE);
          charCount = MAX_RESPONSE_SIZE;
        }
        char responseBuffer[MAX_RESPONSE_SIZE];
        int c = _clock_Wifi->wifiClient.read(responseBuffer, charCount);
        responseBuffer[c] = char(0);

        if (deserializeInternetTime(responseBuffer, charCount)) {

          unixtime = _lastInternetTime.unixtime;

          _internet_update_previousMillis = millis();
          _last_update_successful = true;
        }
        receivedResponse = true;
      }
    }
    
    // Check if connection closed unexpectedly
    if (!receivedResponse && !_clock_Wifi->wifiClient.connected()) {
      DEBUGV("[CLK_RTC] Connection closed by server before receiving response\n");
      break;
    }
  }

  setTime((uint32_t)unixtime);
  if (!foundResponseBody) {
    // _sdcard->writeLogEntry(_logfile, "Timed out waiting for response.", debugLevelName[DebugLevels::Error]);
    DEBUGV("[CLK_RTC] Timed out waiting for response.\n");
  } else if (!_last_update_successful) {
    // _sdcard->writeLogEntry(_logfile, "Time failed to update from the internet.", debugLevelName[DebugLevels::Error]);
    DEBUGV("[CLK_RTC] Time failed to update from the internet.\n");
  } else {
    // adjustTime(_lastInternetTime.raw_offset + ((millis() - _internet_update_previousMillis) / 1000));
    adjustTime(_lastInternetTime.raw_offset);
    if (_lastInternetTime.dst) {
      adjustTime(_lastInternetTime.dst_offset);
    }
    // char buffer[384];
    // sprintf(buffer, "Updated DateTime from: %s", timeUpdateUrl);
    // _sdcard->writeLogEntry(_logfile, buffer, debugLevelName[DebugLevels::Info]);
    DEBUGV("[CLK_RTC] Updated DateTime from: %s\n", timeUpdateUrl);
    char dtBuffer[40];
    getIsoDateString(dtBuffer);
    // sprintf(buffer, "Updated DateTime is: %s", dtBuffer);
    // _sdcard->writeLogEntry(_logfile, buffer, debugLevelName[DebugLevels::Info]);
    DEBUGV("[CLK_RTC] Updated DateTime is: %s\n", dtBuffer);
  }

  // Clean up: close the connection
  _clock_Wifi->wifiClient.stop();

  return now();
}

void Clk_Rtc::getIsoDateString(char stringBuffer[40]) {
  datetime_t dt;
  unixToDatetime_t(&dt, now());
  sprintf(stringBuffer, "%d-%02d-%02dT%02d:%02d:%02d%s", dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, _lastInternetTime.utc_offset);  // 2023-09-23T10:01:25.238839-04:00
}

void Clk_Rtc::getTime(datetime_t *dt) {
  unixToDatetime_t(dt, now());
}

void Clk_Rtc::getTimeString(char stringBuffer[10]) {
  datetime_t dt;
  unixToDatetime_t(&dt, now());
  
  // Optimize: Manual formatting is faster than sprintf for simple time strings
  // Format: "HH:MM:SS"
  stringBuffer[0] = '0' + (dt.hour / 10);
  stringBuffer[1] = '0' + (dt.hour % 10);
  stringBuffer[2] = ':';
  stringBuffer[3] = '0' + (dt.min / 10);
  stringBuffer[4] = '0' + (dt.min % 10);
  stringBuffer[5] = ':';
  stringBuffer[6] = '0' + (dt.sec / 10);
  stringBuffer[7] = '0' + (dt.sec % 10);
  stringBuffer[8] = '\0';
}

bool Clk_Rtc::timeIsSet() {
  return (_last_update_successful && timeStatus() == timeSet);
}

void Clk_Rtc::unixToDatetime_t(datetime_t *mydate, uint32_t unixtime) {
  uint8_t yOff;  ///< Year offset from 2000
  uint8_t m;     ///< Month 1-12
  uint8_t d;     ///< Day 1-31
  uint8_t hh;    ///< Hours 0-23
  uint8_t mm;    ///< Minutes 0-59
  uint8_t ss;    ///< Seconds 0-59

  unixtime -= SECONDS_FROM_1970_TO_2000;  // bring to 2000 timestamp from 1970

  ss = unixtime % 60;
  unixtime /= 60;

  mm = unixtime % 60;
  unixtime /= 60;

  hh = unixtime % 24;
  uint16_t days = unixtime / 24;
  uint8_t leap;

  for (yOff = 0;; ++yOff) {
    leap = yOff % 4 == 0;

    if (days < (uint16_t)(365 + leap))
      break;

    days -= 365 + leap;
  }

  for (m = 1;; ++m) {
    uint8_t daysPerMonth = daysInMonth[m - 1];

    if (leap && m == 2)
      ++daysPerMonth;

    if (days < daysPerMonth)
      break;

    days -= daysPerMonth;
  }

  d = days + 1;

  mydate->year = 2000 + yOff;
  mydate->month = m;
  mydate->day = d;
  mydate->hour = hh;
  mydate->min = mm;
  mydate->sec = ss;
}

// ***** PRIVATE *****
bool Clk_Rtc::deserializeInternetTime(const char *input, size_t inputLength) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, input, inputLength);

  if (error) {
    // char buffer[256] = "Clk_Rtc.getInternetTime() deserializeJson() failed: ";
    // int bufLen = strlen(buffer) + strlen(error.c_str()) + 1;

    // if (bufLen < sizeof(buffer) - 1) {
    //   strcat(buffer, error.c_str());
    // }

    // _sdcard->writeLogEntry(_logfile, buffer, debugLevelName[DebugLevels::Error]);
    DEBUGV("[CLK_RTC] getInternetTime() deserializeJson() failed: %s\n", error.c_str());
    return false;
  }

  // Use strlcpy to safely copy strings and prevent dangling pointers
  strlcpy(_lastInternetTime.abbreviation, doc["abbreviation"] | "", sizeof(_lastInternetTime.abbreviation));
  strlcpy(_lastInternetTime.client_ip, doc["client_ip"] | "", sizeof(_lastInternetTime.client_ip));
  strlcpy(_lastInternetTime.datetime, doc["datetime"] | "", sizeof(_lastInternetTime.datetime));
  _lastInternetTime.day_of_week = doc["day_of_week"] | 0;    // 0
  _lastInternetTime.day_of_year = doc["day_of_year"] | 0;    // 260
  _lastInternetTime.dst = doc["dst"] | false;                // true
  strlcpy(_lastInternetTime.dst_from, doc["dst_from"] | "", sizeof(_lastInternetTime.dst_from));
  _lastInternetTime.dst_offset = doc["dst_offset"] | 0;      // 3600
  strlcpy(_lastInternetTime.dst_until, doc["dst_until"] | "", sizeof(_lastInternetTime.dst_until));
  _lastInternetTime.raw_offset = doc["raw_offset"] | 0;      // -18000
  strlcpy(_lastInternetTime.timezone, doc["timezone"] | "", sizeof(_lastInternetTime.timezone));
  _lastInternetTime.unixtime = doc["unixtime"] | 0;          // 1694987696
  strlcpy(_lastInternetTime.utc_datetime, doc["utc_datetime"] | "", sizeof(_lastInternetTime.utc_datetime));
  strlcpy(_lastInternetTime.utc_offset, doc["utc_offset"] | "+00:00", sizeof(_lastInternetTime.utc_offset));
  _lastInternetTime.week_number = doc["week_number"] | 0;    // 37

  return true;
}