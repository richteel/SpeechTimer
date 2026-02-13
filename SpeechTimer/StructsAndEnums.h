#ifndef STRUCTSANDENUMS
#define STRUCTSANDENUMS

#if defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
#include <FreeRTOS.h>
#include <task.h>
#define xPortGetCoreID get_core_num
#endif

#include <map>
#include <WiFi.h>


enum class DebugLevels {
  Verbose = 0,
  Info = 1,
  Warning = 2,
  Error = 3
};


enum class ClockMode {
  Clock,        // Remote and Web
  TimerReady,   // Remote and Web
  TimerSetMin,  // Remote only
  TimerSetMax,  // Remote only
  TimerRun,     // Remote and Web
  TimerStop,    // Remote and Web
  TestColors,   // Web Only
  TestRainbow   // Web Only
};

/*****************************************************************************
 *                                  MAPPINGS                                 *
 *****************************************************************************/
// Mapping for string lookup
static std::map<WiFiMode_t, const char*> wifiModeName{
  { WIFI_OFF, "WiFi Off" },
  { WIFI_STA, "Station Mode" },
  { WIFI_AP, "Soft-AP Mode" },
  { WIFI_AP_STA, "Station + Soft-AP Mode" }
};
static std::map<eTaskState, const char*> eTaskStateName{
  { eReady, "Ready" },
  { eRunning, "Running" },
  { eBlocked, "Blocked" },
  { eSuspended, "Suspended" },
  { eDeleted, "Deleted" }
};
static std::map<DebugLevels, const char*> debugLevelName{
  { DebugLevels::Verbose, "Verbose" },
  { DebugLevels::Info, "Info" },
  { DebugLevels::Warning, "Warning" },
  { DebugLevels::Error, "Error" }
};
static std::map<ClockMode, const char*> debugClockModeName{
  { ClockMode::Clock, "Clock" },
  { ClockMode::TimerReady, "Timer Ready" },
  { ClockMode::TimerSetMin, "Timer Set Min" },
  { ClockMode::TimerSetMax, "Timer Set Max" },
  { ClockMode::TimerRun, "Timer Run" },
  { ClockMode::TimerStop, "Timer Stop" },
  { ClockMode::TestColors, "Test Colors" },
  { ClockMode::TestRainbow, "Test Rainbow" }
};

// Structure returned by https://worldtimeapi.org/api/ip and https://worldtimeapi.org/api/timezone/America/New_York
struct InternetTimeStruct {
  char abbreviation[8];           // "EDT" - Changed from const char* to prevent dangling pointer
  char client_ip[16];             // "173.79.173.100" - Changed from const char*
  char datetime[64];              // "2023-09-17T17:54:56.042498-04:00" - Changed from const char*
  int day_of_week;                // 0
  int day_of_year;                // 260
  bool dst;                       // true
  char dst_from[64];              // "2023-03-12T07:00:00+00:00" - Changed from const char*
  int dst_offset;                 // 3600
  char dst_until[64];             // "2023-11-05T06:00:00+00:00" - Changed from const char*
  int raw_offset;                 // -18000
  char timezone[64];              // "America/New_York" - Changed from const char*
  long unixtime;                  // 1694987696
  char utc_datetime[64];          // "2023-09-17T21:54:56.042498+00:00" - Changed from const char*
  char utc_offset[8];             // "-04:00" - Changed from const char*, default "+00:00"
  int week_number;                // 37
};

struct Log_Entry {
  char message[512];
  char logfile[128];
};  // logEntry;


#endif  // STRUCTSANDENUMS