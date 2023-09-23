#include "Clock_Rtc.h"

// ***** PUBLIC *****
Clock_Rtc::Clock_Rtc(Clock_SdCard *sdcard, Clock_Wifi *clock_Wifi, unsigned long internet_update_interval) {
  _sdcard = sdcard;
  _clock_Wifi = clock_Wifi;
  _internet_update_interval = internet_update_interval;
}

bool Clock_Rtc::begin() {
  setSyncInterval(14400);              // Update the time every 4 hours.
                                       // After 4 hours, calling timeStatus() will return timeNeedsSync.
  setTime(SECONDS_FROM_1970_TO_2000);  // Sat Jan 01 2000 00:00:00 GMT+0000

  return true;
}

long Clock_Rtc::getInternetTime(long timeoutMs) {
  long unixtime = 0;
  unsigned long startConnection;
  const char server[] = "worldtimeapi.org";
  bool receivedResponse = false;

  if (!_clock_Wifi->isConnectedToInternet()) {
    return unixtime;
  }

  // if you get a connection, report back via serial:
  if (_clock_Wifi->wifiClient.connect(server, 80)) {
    if (strlen(_sdcard->config.timezone) == 0) {
      _clock_Wifi->wifiClient.println(F("GET /api/ip/? HTTP/1.1"));
      sprintf(timeUpdateUrl, "http://%s/api/ip/?", server);
    } else {
      _clock_Wifi->wifiClient.printf("GET /api/timezone/%s/? HTTP/1.1\n", _sdcard->config.timezone);
      sprintf(timeUpdateUrl, "http://%s/api/timezone/%s/?", server, _sdcard->config.timezone);
    }
    _clock_Wifi->wifiClient.println(F("Host: worldtimeapi.org"));
    _clock_Wifi->wifiClient.println(F("User-Agent: Lynx/2.8.9dev libwww-FM/2.14 SSL-MM/1.4.1 GNUTLS/3.3.8 (Raspberry Pi Pico W)"));
    _clock_Wifi->wifiClient.println(F("Accept: text/html,application/json"));
    _clock_Wifi->wifiClient.println(F("Connection: close"));
    _clock_Wifi->wifiClient.println();
  }

  startConnection = millis();
  bool foundResponseBody = false;
  bool deserializedData = false;
  char lastchars[5] = "\0\0\0\0";

  while (!receivedResponse && (millis() - startConnection <= timeoutMs)) {
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
        int charCount = _clock_Wifi->wifiClient.available();
        char responseBuffer[charCount + 1];
        int c = _clock_Wifi->wifiClient.read(responseBuffer, charCount);
        responseBuffer[c] = char(0);

        if (deserializeInternetTime(responseBuffer, charCount)) {

          unixtime = _lastInternetTime.unixtime;

          deserializedData = true;
          _internet_update_previousMillis = millis();
        }
        receivedResponse = true;
      }
    }
  }

  if (!foundResponseBody) {
    _sdcard->writeLogEntry(_logfile, "Timed out waiting for response.", LOG_ERROR);
  }

  setTime((uint32_t)unixtime);
  adjustTime(_lastInternetTime.raw_offset + ((millis() - _internet_update_previousMillis) / 1000));
  if (_lastInternetTime.dst) {
    adjustTime(_lastInternetTime.dst_offset);
  }

  return now();
}

void Clock_Rtc::getIsoDateString(char stringBuffer[40]) {
  datetime_t dt;
  unixToDatetime_t(&dt, now());
  sprintf(stringBuffer, "%d-%02d-%02dT%02d:%02d:%02d%s", dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, _lastInternetTime.utc_offset);  // 2023-09-23T10:01:25.238839-04:00
}

void Clock_Rtc::getTime(datetime_t *dt) {
  unixToDatetime_t(dt, now());
}

void Clock_Rtc::getTimeString(char stringBuffer[10]) {
  datetime_t dt;
  unixToDatetime_t(&dt, now());
  sprintf(stringBuffer, "%02d:%02d:%02d", dt.hour, dt.min, dt.sec);  // 2023-09-23T10:01:25.238839-04:00
}

void Clock_Rtc::unixToDatetime_t(datetime_t *mydate, uint32_t unixtime) {
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
bool Clock_Rtc::deserializeInternetTime(const char *input, size_t inputLength) {
  StaticJsonDocument<768> doc;

  DeserializationError error = deserializeJson(doc, input, inputLength);

  if (error) {
    char buffer[256] = "Clock_Rtc.getInternetTime() deserializeJson() failed: ";
    int bufLen = strlen(buffer) + strlen(error.c_str()) + 1;

    if (bufLen < sizeof(buffer) - 1) {
      strcat(buffer, error.c_str());
    }

    _sdcard->writeLogEntry(_logfile, buffer, LOG_ERROR);
    return false;
  }

  _lastInternetTime.abbreviation = doc["abbreviation"];  // "EDT"
  _lastInternetTime.client_ip = doc["client_ip"];        // "173.79.173.100"
  _lastInternetTime.datetime = doc["datetime"];          // "2023-09-17T17:54:56.042498-04:00"
  _lastInternetTime.day_of_week = doc["day_of_week"];    // 0
  _lastInternetTime.day_of_year = doc["day_of_year"];    // 260
  _lastInternetTime.dst = doc["dst"];                    // true
  _lastInternetTime.dst_from = doc["dst_from"];          // "2023-03-12T07:00:00+00:00"
  _lastInternetTime.dst_offset = doc["dst_offset"];      // 3600
  _lastInternetTime.dst_until = doc["dst_until"];        // "2023-11-05T06:00:00+00:00"
  _lastInternetTime.raw_offset = doc["raw_offset"];      // -18000
  _lastInternetTime.timezone = doc["timezone"];          // "America/New_York"
  _lastInternetTime.unixtime = doc["unixtime"];          // 1694987696
  _lastInternetTime.utc_datetime = doc["utc_datetime"];  // "2023-09-17T21:54:56.042498+00:00"
  _lastInternetTime.utc_offset = doc["utc_offset"];      // "-04:00"
  _lastInternetTime.week_number = doc["week_number"];    // 37

  return true;
}