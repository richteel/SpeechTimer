#include "Clk_Rtc.h"
#include "StructsAndEnums.h"
#include "Defines.h"  // For constants
#include "DbgPrint.h" // For Serial debugging
#include <ctype.h>     // For isspace
#include <time.h>      // For NTP time functions

// Route RTC log writes through the log queue.
#define writeLogEntry queueLogEntry

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
  if (!_clock_Wifi->hasIpAddress()) {
    return now();
  }

  // Check if we need to perform an update (based on update interval)
  unsigned long currentMillis = millis();

  // Check if we need to detect timezone
  // Retry faster until we get a successful detection, then switch to hourly checks
  unsigned long timezoneInterval = _timezone_last_success ? _timezone_check_interval : _timezone_retry_interval;
  bool needTimezoneCheck = (_timezone_check_previousMillis == 0) ||
                           (currentMillis - _timezone_check_previousMillis >= timezoneInterval);
  bool throttled = currentMillis < _timezone_throttle_until;
  char msgBuffer[256];
  if (throttled && !_timezone_throttle_logged) {
    sprintf(msgBuffer, "*** Timezone detection throttled until %lu ms", _timezone_throttle_until);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    Dbg_println(msgBuffer);
    _timezone_throttle_logged = true;
  }
  
  // Try to detect timezone from IP (not blocking - use strict timeout)
  // This is separate from NTP sync to avoid blocking time synchronization
  if (needTimezoneCheck && !throttled) {
    _sdcard->writeLogEntry(_logfile, "Detecting timezone from IP (max 3 sec timeout)...");
    Dbg_println("Detecting timezone from IP (max 3 sec timeout)...");
    
    unsigned long tzStartTime = millis();
    if (detectTimezoneFromIP()) {
      _timezone_check_previousMillis = currentMillis;
      _timezone_last_success = true;
      _timezone_throttle_logged = false;
      unsigned long tzElapsed = millis() - tzStartTime;
      sprintf(msgBuffer, "*** Timezone detected in %lu ms: %s (offset: %d seconds = %d hours)", 
              tzElapsed, _detected_timezone, _timezone_offset_seconds, _timezone_offset_seconds / 3600);
      _sdcard->writeLogEntry(_logfile, msgBuffer);
      Dbg_println(msgBuffer);
    } else {
      unsigned long tzElapsed = millis() - tzStartTime;
      _timezone_check_previousMillis = currentMillis;
      _timezone_last_success = false;
      if (_timezone_detected) {
        sprintf(msgBuffer, "*** Failed to detect timezone in %lu ms, keeping last: %s (offset: %d seconds = %+.1f hours)",
                tzElapsed, _detected_timezone, _timezone_offset_seconds, _timezone_offset_seconds / 3600.0);
        _sdcard->writeLogEntry(_logfile, msgBuffer);
        Dbg_println(msgBuffer);
      } else {
        sprintf(msgBuffer, "*** Failed to detect timezone in %lu ms, will use UTC", tzElapsed);
        _sdcard->writeLogEntry(_logfile, msgBuffer);
        Dbg_println(msgBuffer);
        _timezone_offset_seconds = 0;
      }
    }
  }

  // Skip NTP sync if not enough time has passed (unless this is the first update)
  if (_last_update_successful && 
      (currentMillis - _internet_update_previousMillis < _internet_update_interval)) {
    // Throttled - not logging to avoid spam
    return now();  // Not time for update yet
  }

  // Log that we're starting an update cycle
  if (_last_update_successful) {
    sprintf(msgBuffer, "=== Starting periodic time sync (last: %lu ms ago) ===", 
            currentMillis - _internet_update_previousMillis);
  } else {
    sprintf(msgBuffer, "=== Starting FIRST time sync ===");
  }
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  Dbg_println(msgBuffer);  // Also log to Serial for debugging

  // Perform NTP time synchronization
  bool syncSuccess = syncTimeWithNTP(timeoutMs);
  
  if (syncSuccess) {
    _last_update_successful = true;  // Only set to true on success
    _ever_synced = true;
    _internet_update_previousMillis = millis();
    
    sprintf(msgBuffer, "Time synchronized via NTP");
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    Dbg_println(msgBuffer);
    
    char dtBuffer[40];
    getIsoDateString(dtBuffer);
    sprintf(msgBuffer, "Current DateTime: %s", dtBuffer);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    Dbg_println(msgBuffer);
  } else {
    _sdcard->writeLogEntry(_logfile, "Failed to synchronize time via NTP");
    Dbg_println("Failed to synchronize time via NTP");
    // Keep _last_update_successful as is - don't reset to false if it was true before
    // This prevents the clock from blanking if NTP temporarily fails
  }

  return now();
}

bool Clk_Rtc::syncTimeWithNTP(long timeoutMs) {
  // NTP server configuration
  const char* ntpServer1 = "pool.ntp.org";
  const char* ntpServer2 = "time.nist.gov";
  
  // Determine timezone offset to use (store in member variable for logging)
  int gmtOffset_sec = 0;
  int daylightOffset_sec = 0;  // DST offset, if applicable
  const char* tzString = "UTC";
  char msgBuffer[256];
  
  // Always prefer detected numeric offset from IP API (works reliably with configTime)
  if (_timezone_detected) {
    // Use detected numeric offset from IP API
    gmtOffset_sec = _timezone_offset_seconds;
    tzString = _detected_timezone;
    sprintf(msgBuffer, "*** Using detected timezone: %s with offset: %d seconds (%+.1f hours)", 
           tzString, gmtOffset_sec, gmtOffset_sec / 3600.0);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
  } else {
    // No timezone info available at all
    gmtOffset_sec = 0;
    tzString = "UTC";
    _sdcard->writeLogEntry(_logfile, "*** No timezone info available, defaulting to UTC (offset: 0)");
  }
  
  // Configure NTP with timezone offset
  // Note: configTime() on Pico W/ESP-style platforms takes GMT offset in seconds
  sprintf(msgBuffer, "*** Calling configTime with GMT offset: %d seconds", gmtOffset_sec);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  
  // Set timezone using POSIX TZ format
  setenv("TZ", tzString, 1);
  tzset();
  
  // Wait for time synchronization with timeout
  unsigned long startTime = millis();
  time_t nowSecs = time(nullptr);
  
  // Unix timestamp for Jan 1, 2020 is 1577836800
  // Check more frequently (50ms) for faster sync detection
  while (nowSecs < 1577836800 && (millis() - startTime < (unsigned long)timeoutMs)) {
    delay(50);
    nowSecs = time(nullptr);
  }
  
  if (nowSecs < 1577836800) {
    char msgBuffer[128];
    sprintf(msgBuffer, "*** NTP sync timeout (time still: %ld)", nowSecs);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    return false;
  }
  
  sprintf(msgBuffer, "*** NTP time received: %ld (Unix timestamp UTC)", nowSecs);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  Dbg_println(msgBuffer);
  
  // Update TimeLib with the synchronized time
  // Note: On Pico W, time(nullptr) returns UTC. We need to manually apply timezone offset.
  time_t localTime = nowSecs + gmtOffset_sec;
  sprintf(msgBuffer, "*** Setting TimeLib to: %ld (UTC %+d = local)", localTime, gmtOffset_sec);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  Dbg_println(msgBuffer);
  setTime((uint32_t)localTime);
  
  // Calculate timezone offset by comparing local time with UTC
  // Get both UTC and local time
  time_t utcTime = time(nullptr);
  struct tm utcTimeinfo;
  struct tm localTimeinfo;
  
  gmtime_r(&utcTime, &utcTimeinfo);
  localtime_r(&utcTime, &localTimeinfo);
  
  // Calculate offset in seconds
  // Convert both to seconds since epoch and find difference
  time_t utc_seconds = mktime(&utcTimeinfo);
  time_t local_seconds = mktime(&localTimeinfo);
  
  // Store timezone information
  int effective_offset = 0;
  
  if (_timezone_detected) {
    // Use the offset from IP API detection (most reliable)
    effective_offset = _timezone_offset_seconds;
    sprintf(msgBuffer, "*** Using IP-detected offset: %d seconds", effective_offset);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
  } else {
    // Try to calculate from time difference (fallback)
    effective_offset = (int)(local_seconds - utc_seconds);
    sprintf(msgBuffer, "*** Calculated offset from time diff: %d seconds", effective_offset);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    if (effective_offset == 0 && gmtOffset_sec != 0) {
      // Use the gmtOffset we configured if calculation failed
      effective_offset = gmtOffset_sec;
      sprintf(msgBuffer, "*** Time diff calculation failed, using gmtOffset: %d seconds", effective_offset);
      _sdcard->writeLogEntry(_logfile, msgBuffer);
    }
  }
  
  // Calculate and store offset string
  int offset_hours = effective_offset / 3600;
  int offset_mins = abs(effective_offset % 3600) / 60;
  snprintf(_lastInternetTime.utc_offset, sizeof(_lastInternetTime.utc_offset), 
           "%+03d:%02d", offset_hours, offset_mins);
  
  strlcpy(_lastInternetTime.timezone, tzString, sizeof(_lastInternetTime.timezone));
  _lastInternetTime.unixtime = nowSecs;
  
  // Log detailed sync info
  _sdcard->writeLogEntry(_logfile, "*** NTP sync successful!");
  sprintf(msgBuffer, "***   Unix time: %ld", nowSecs);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  sprintf(msgBuffer, "***   Timezone: %s", tzString);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  sprintf(msgBuffer, "***   UTC offset: %s (%d seconds)", _lastInternetTime.utc_offset, effective_offset);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  
  return true;
}

bool Clk_Rtc::detectTimezoneFromIP() {
  const char server[] = "ip-api.com";
  bool success = false;
  char msgBuffer[256];
  
  _sdcard->writeLogEntry(_logfile, "*** Starting timezone detection from ip-api.com...");
  Dbg_println("*** Starting timezone detection from ip-api.com...");
  
  unsigned long connectStart = millis();
  bool connected = false;
  
  // Try to connect with STRICT timeout (1 second max) to avoid blocking NTP
  while (!connected && (millis() - connectStart <= 1000)) {
    if (_clock_Wifi->wifiClient.connect(server, 80)) {
      connected = true;
      break;
    }
    delay(HTTP_CONNECTION_RETRY_DELAY_MS);
  }
  
  if (!connected) {
    sprintf(msgBuffer, "*** FAILED to connect to %s for timezone detection", server);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    Dbg_println(msgBuffer);
    return false;
  }
  
  sprintf(msgBuffer, "*** Connected to %s, sending request...", server);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  Dbg_println(msgBuffer);
  
  // Send HTTP request for timezone info
  // Using fields parameter to get only what we need: timezone, offset
  _clock_Wifi->wifiClient.println(F("GET /json/?fields=status,timezone,offset HTTP/1.1"));
  _clock_Wifi->wifiClient.println(F("Host: ip-api.com"));
  _clock_Wifi->wifiClient.println(F("User-Agent: SpeechTimer/1.0 (Raspberry Pi Pico W)"));
  _clock_Wifi->wifiClient.println(F("Accept: application/json"));
  _clock_Wifi->wifiClient.println(F("Connection: close"));
  _clock_Wifi->wifiClient.println();
  
  unsigned long startConnection = millis();
  constexpr size_t MAX_RESPONSE_SIZE = 512;
  char responseBuffer[MAX_RESPONSE_SIZE];
  size_t total = 0;

  // Read full response with STRICT timeout (1.5 seconds max to avoid blocking NTP)
  while (millis() - startConnection <= 1500) {
    while (_clock_Wifi->wifiClient.available()) {
      int c = _clock_Wifi->wifiClient.read();
      if (c < 0) {
        break;
      }
      if (total < MAX_RESPONSE_SIZE - 1) {
        responseBuffer[total++] = static_cast<char>(c);
      }
    }

    if (!_clock_Wifi->wifiClient.connected() && !_clock_Wifi->wifiClient.available()) {
      break;
    }
    delay(10);
  }

  responseBuffer[total] = '\0';

  if (total == 0) {
    _sdcard->writeLogEntry(_logfile, "*** Timeout waiting for timezone response");
  } else {
    const char *lineEnd = strstr(responseBuffer, "\r\n");
    int httpStatus = 0;
    if (lineEnd != nullptr) {
      char statusLine[64];
      size_t lineLen = lineEnd - responseBuffer;
      if (lineLen >= sizeof(statusLine)) {
        lineLen = sizeof(statusLine) - 1;
      }
      strncpy(statusLine, responseBuffer, lineLen);
      statusLine[lineLen] = '\0';
      if (sscanf(statusLine, "HTTP/%*s %d", &httpStatus) == 1) {
        sprintf(msgBuffer, "*** IP-API HTTP status: %d", httpStatus);
        _sdcard->writeLogEntry(_logfile, msgBuffer);
      }
    }

    if (httpStatus == 429) {
      sprintf(msgBuffer, "*** IP-API throttled (429). Backing off for %lu ms", _timezone_throttle_interval);
      _sdcard->writeLogEntry(_logfile, msgBuffer);
      Dbg_println(msgBuffer);
      _timezone_throttle_until = millis() + _timezone_throttle_interval;
      _timezone_throttle_logged = false;
    } else {
      const char *body = strstr(responseBuffer, "\r\n\r\n");
      if (body == nullptr) {
        _sdcard->writeLogEntry(_logfile, "*** Failed to locate HTTP response body");
      } else {
        body += 4;
        while (*body != '\0' && isspace(static_cast<unsigned char>(*body))) {
          ++body;
        }

        char *bodyEnd = strrchr(const_cast<char*>(body), '}');
        if (*body != '{' || bodyEnd == nullptr) {
          _sdcard->writeLogEntry(_logfile, "*** Response body does not contain valid JSON object");
        } else {
          bodyEnd[1] = '\0';
          sprintf(msgBuffer, "*** IP-API Response Body: %s", body);
          _sdcard->writeLogEntry(_logfile, msgBuffer);
          Dbg_println(msgBuffer);

          if (deserializeTimezoneInfo(body, strlen(body))) {
            success = true;
            _timezone_detected = true;
            _sdcard->writeLogEntry(_logfile, "*** Timezone info parsed successfully");
            Dbg_println("*** Timezone info parsed successfully");
          } else {
            _sdcard->writeLogEntry(_logfile, "*** Failed to parse timezone info");
            Dbg_println("*** Failed to parse timezone info");
          }
        }
      }
    }
  }
  
  _clock_Wifi->wifiClient.stop();
  return success;
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
  // Unix timestamp for Jan 1, 2020 is 1577836800
  return _ever_synced && now() >= 1577836800;
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
    char msgBuffer[256];
    sprintf(msgBuffer, "getInternetTime() deserializeJson() failed: %s", error.c_str());
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    return false;
  }

  // Check if the response contains an error (e.g., API sunset, service unavailable)
  if (!doc["error"].isNull()) {
    const char* errorType = doc["error"] | "unknown";
    const char* errorMsg = doc["message"] | "No message provided";
    int statusCode = doc["status"] | 0;
    char msgBuffer[256];
    sprintf(msgBuffer, "API Error Response - Status: %d, Error: %s, Message: %s", statusCode, errorType, errorMsg);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    return false;
  }

  // Validate that we have the required unixtime field with a reasonable value
  // Unix timestamp for Jan 1, 2020 is 1577836800
  long unixtime = doc["unixtime"] | 0;
  if (unixtime < 1577836800) {
    char msgBuffer[128];
    sprintf(msgBuffer, "Invalid or missing unixtime in response: %ld", unixtime);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
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
  _lastInternetTime.unixtime = unixtime;                     // 1694987696
  strlcpy(_lastInternetTime.utc_datetime, doc["utc_datetime"] | "", sizeof(_lastInternetTime.utc_datetime));
  strlcpy(_lastInternetTime.utc_offset, doc["utc_offset"] | "+00:00", sizeof(_lastInternetTime.utc_offset));
  _lastInternetTime.week_number = doc["week_number"] | 0;    // 37

  return true;
}

bool Clk_Rtc::deserializeTimezoneInfo(const char *input, size_t inputLength) {
  JsonDocument doc;
  char msgBuffer[256];
  
  sprintf(msgBuffer, "*** Parsing timezone JSON (length: %d)", inputLength);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  
  DeserializationError error = deserializeJson(doc, input, inputLength);
  
  if (error) {
    sprintf(msgBuffer, "*** deserializeTimezoneInfo() JSON parse failed: %s", error.c_str());
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    return false;
  }
  
  // Check if the response is successful
  const char* status = doc["status"] | "fail";
  if (strcmp(status, "success") != 0) {
    sprintf(msgBuffer, "*** IP-API returned non-success status: %s", status);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    const char* message = doc["message"] | "No error message";
    sprintf(msgBuffer, "***   Message: %s", message);
    _sdcard->writeLogEntry(_logfile, msgBuffer);
    return false;
  }
  
  // Extract timezone and offset
  const char* timezone = doc["timezone"] | "";
  int offset = doc["offset"] | 0;  // Offset in seconds from UTC
  
  if (strlen(timezone) == 0) {
    _sdcard->writeLogEntry(_logfile, "*** No timezone field in response");
    return false;
  }
  
  // Store the detected timezone
  strlcpy(_detected_timezone, timezone, sizeof(_detected_timezone));
  _timezone_offset_seconds = offset;
  
  _sdcard->writeLogEntry(_logfile, "*** Timezone info parsed successfully:");
  sprintf(msgBuffer, "***   Timezone: %s", _detected_timezone);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  sprintf(msgBuffer, "***   Offset: %d seconds (%+.1f hours)", 
         _timezone_offset_seconds, _timezone_offset_seconds / 3600.0);
  _sdcard->writeLogEntry(_logfile, msgBuffer);
  
  return true;
}