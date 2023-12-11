#ifndef CLK_RTC_H
#define CLK_RTC_H

#include <ArduinoJson.h>
#include <TimeLib.h>

#include "config.h"
#include "Clk_SdCard.h"
#include "Clk_Wifi.h"
#include "StructsAndEnums.h"

class Clk_Rtc {
public:
  // Constructor:
  Clk_Rtc(Clk_SdCard *sdcard, Clk_Wifi *clock_Wifi, unsigned long internet_update_interval = 60000);

  bool begin();

  long getInternetTime(long timeoutMs = 2000);

  void getIsoDateString(char stringBuffer[40]);

  void getTime(datetime_t *currentTime);

  void getTimeString(char stringBuffer[10]);

  void setTimer(int min, int max);

  bool timeIsSet(void);

  void unixToDatetime_t(datetime_t *mydate, uint32_t unixtime);

  char timeUpdateUrl[256] = {};

  static constexpr uint32_t SECONDS_FROM_1970_TO_2000{ 946684800 };
private:
  const uint8_t daysInMonth[12]{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  bool _last_update_successful = false;

  Clk_SdCard *_sdcard;
  Clk_Wifi *_clock_Wifi;
  unsigned long _internet_update_previousMillis = 0;
  unsigned long _internet_update_interval;
  const char *_logfile = "logs/rtc.log";

  InternetTimeStruct _lastInternetTime;

  bool deserializeInternetTime(const char *input, size_t inputLength = 0);
};

#endif  // CLK_RTC_H