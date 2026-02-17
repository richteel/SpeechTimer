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
  
  bool syncTimeWithNTP(long timeoutMs = 5000);
  
  bool detectTimezoneFromIP();

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
  bool _ever_synced = false;

  Clk_SdCard *_sdcard;
  Clk_Wifi *_clock_Wifi;
  unsigned long _internet_update_previousMillis = 0;
  unsigned long _internet_update_interval;
  unsigned long _timezone_check_previousMillis = 0;
  unsigned long _timezone_check_interval = 3600000; // 1 hour in milliseconds
  unsigned long _timezone_retry_interval = 15000;   // 15 seconds when not detected yet
  unsigned long _timezone_throttle_interval = 60000; // 60 seconds after HTTP 429
  unsigned long _timezone_throttle_until = 0;
  bool _timezone_last_success = false;
  bool _timezone_throttle_logged = false;
  const char *_logfile = "logs/rtc.log";
  
  char _detected_timezone[64] = {};
  int _timezone_offset_seconds = 0;
  bool _timezone_detected = false;

  InternetTimeStruct _lastInternetTime;

  bool deserializeInternetTime(const char *input, size_t inputLength = 0);
  bool deserializeTimezoneInfo(const char *input, size_t inputLength);
};

#endif  // CLK_RTC_H