#ifndef CLOCK_RTC_H
#define CLOCK_RTC_H

#include <ArduinoJson.h>
#include <TimeLib.h>

#include "config.h"
#include "Clock_SdCard.h"
#include "Clock_Wifi.h"
#include "InternetTimeStruct.h"

class Clock_Rtc {
public:
  // Constructor:
  Clock_Rtc(Clock_SdCard *sdcard, Clock_Wifi *clock_Wifi, unsigned long internet_update_interval = 60000);

  bool begin();

  long getInternetTime(long timeoutMs = 2000);

  void getIsoDateString(char stringBuffer[40]);

  void getTime(datetime_t *currentTime);

  void getTimeString(char stringBuffer[10]);

  bool timeIsSet(void);

  void unixToDatetime_t(datetime_t *mydate, uint32_t unixtime);

  char timeUpdateUrl[256] = {};

  static constexpr uint32_t SECONDS_FROM_1970_TO_2000{ 946684800 };
private:
  const uint8_t daysInMonth[12]{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  bool _last_update_successful = false;

  Clock_SdCard *_sdcard;
  Clock_Wifi *_clock_Wifi;
  unsigned long _internet_update_previousMillis = 0;
  unsigned long _internet_update_interval;
  const char *_logfile = "logs/rtc.log";

  InternetTimeStruct _lastInternetTime;

  bool deserializeInternetTime(const char *input, size_t inputLength = 0);
};

#endif