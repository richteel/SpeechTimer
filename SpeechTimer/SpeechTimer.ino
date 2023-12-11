/*
 SpeechTimer.ino

  AUTHOR: Richard Teel
  DATE: 15 October 2023
  HW TESTED: Raspberry Pi Pico W

  Speech Timer for Toastmasters Club or any event that needs to display a timer with minutes and seconds.
  ** Features **
    - Clock is displayed if there is an internet connection and the timer is not running.
    - Web interface for operation and configuration.
    - IR Remote for simpler control with speech timer presets of:
      - 1 to 2 minutes for Table Topics
      - 5 to 7 minutes for Speech (default)
      - 2 to 3 minutes for Evaluation
      - Ability to manually set a timer
    - Timer digits change color based on timer settings
      - Digits are white when started
      - Digits are green when the minimum time is reached
      - Digits are yellow when the halfway between the minimum and maximum times
      - Digits are red when the maximum time is reached and stay red
  
  ** Issues/Future Improvements **
  These items are known and planed to be worked on in the future but there is no guarantee that any of 
  them will be addressed.
    - When starting, it may take a minute or longer before the timer function is available. This is due 
      to the time it takes to attempt to set up a Wi-Fi connection. Startup may happen as quickly as 10 
      seconds but may last up to 2 minutes.
    - An auxiliary clock display is planned to allow the same information to be displayed with one 
      positioned for the speaker and the other for the timer/audience.
  
  ** HW Device Connection on Raspberry Pi Pico W **
    Pin GPIO  Device  Device Pin/Function
    6   GP4   OLED      SDA
    7   GP5   OLED      SCK
    20  GP15  Indicator LED
    21  GP16  SD Card   MISO
    22  GP17  SD Card   CS
    24  GP18  SD Card   SCK
    25  GP19  SD Card   MOSI
    26  GP20  SD Card   CD
    29  GP22  IR LED
    31  GP26  IR Remote Signal
    32  GP27  Light Detector (CdS Photoresistor)
    34  GP28  NeoPixels Data

  Power
    NeoPixels - GND and VBUS (Pin 40)
    SD Card   - GND and 3V (Pin 36)
    OLED      - GND and 3V (Pin 36)
    IR Remote - GND and 3V (Pin 36)

  Other References
  - Running code on both threads of the RP2040: https://arduino-pico.readthedocs.io/en/latest/multicore.html
  - Explaination of mutex and semaphore: https://forum.arduino.cc/t/mutex-and-semaphore/1078290/4
  - Example with RTOS semaphore: https://github.com/garyexplains/examples/blob/master/Dual_Core_for_RP2040_ESP32_ESP32-S3/unified_dual_core_with_lock/unified_dual_core_with_lock.ino
    Video explaining code is at: https://youtu.be/w5YigjvSaF4?si=kgyumJVpnzHefH1o
  - BLE with WiFi and FreeRTOS on Raspberry Pi Pico-W: https://mcuoneclipse.com/2023/03/19/ble-with-wifi-and-freertos-on-raspberry-pi-pico-w/
*/

#define FILE_NAME "[SpeechTimer.ino]"

/*****************************************************************************
 *                              FreeRTOS Setup                               *
 *****************************************************************************/
#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_MBED_RP2040) && !defined(ARDUINO_ARCH_RP2040)
#pragma message("Unsupported platform")
#endif

// ESP32:
#if defined(ESP_PLATFORM)
TaskHandle_t task_loop1;
void esploop1(void *pvParameters) {
  setup1();

  for (;;)
    loop1();
}
#endif

#if defined(ARDUINO_ARCH_MBED_RP2040) || defined(ARDUINO_ARCH_RP2040)
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#define xPortGetCoreID get_core_num
#endif

/*****************************************************************************
 *                               Include Files                               *
 *****************************************************************************/
#include <Dictionary.h>

/*****************************************************************************
 *                               Project Files                               *
 *****************************************************************************/
#include "DbgPrint.h"  // Serial helpers
#include "Defines.h"   // Pin definitions, programming mode, and debug flag
#include "Clk_Output.h"
#include "Clk_Remote.h"
#include "config.h"      // Structures for the configuration file
#include "Clk_SdCard.h"  // Higher level SD Card functions
#include "Clk_Wifi.h"    // Higher level Wi-Fi functions
#include "Clk_Rtc.h"
#include "StructsAndEnums.h"

/*****************************************************************************
 *                               Other Includes                              *
 *****************************************************************************/
// #include <map>

/*****************************************************************************
 *                                 DICTIONARY                                *
 *****************************************************************************/
/*
  Dictionary Keys
    - cpu_freq
    - cpu_temp_c
    - cpu_temp_f
    - cpu_uid
    - cpu_voltage
    - max_time
    - mem_free
    - mem_percent_free
    - mem_percent_used
    = mem_total
    - mem_used
    - min_time
    - neo_blue
    - neo_green
    - neo_red
    - reset_reason
    - selected_tab
    - server_folder
    - timer_button_text
    - visits
*/
// Dictionary* dict = new Dictionary(20);

/*****************************************************************************
 *                                  GLOBALS                                  *
 *****************************************************************************/
// Flags to make certain that required initialization is complete before tasks start
bool setup_complete = false;

// WiFi global settings
char last_ipaddress[16] = "";
WiFiMode_t last_WiFiMode = WIFI_OFF;

// Config global settings
unsigned long configLoadMillis = 0;
unsigned long last_configLoadMillis = 0;

// Module Objects
Clk_SdCard clockSdCard = Clk_SdCard();
Clk_Wifi clockWifi = Clk_Wifi();
Clk_Output clockOutput = Clk_Output(&clockSdCard);
Clk_Rtc clockRtc = Clk_Rtc(&clockSdCard, &clockWifi);
Clk_Remote clockRemote = Clk_Remote();
ClockMode last_clockMode = ClockMode::Clock;

/*****************************************************************************
 *                                 LOG FILES                                 *
 *****************************************************************************/
char logDebug[] = "/logs/debug.log";
char logDebugArchive[] = "/logs/debug_1.log";

/*****************************************************************************
 *                                DEBUG LEVEL                                *
 *****************************************************************************/
DebugLevels debugLevel = DebugLevels::Verbose;
// DebugLevels debugLevel = DebugLevels::Info;
// DebugLevels debugLevel = DebugLevels::Warning;
// DebugLevels debugLevel = DebugLevels::Error;

int waitLoopsToPrintTaskInfo = 100;

/*****************************************************************************
 *                                   QUEUES                                  *
 *****************************************************************************/
static const uint8_t log_queue_len = 10;
static QueueHandle_t log_queue;

static const uint8_t remote_queue_len = 5;
static QueueHandle_t remote_queue;

/*****************************************************************************
 *                                   MUTEX                                   *
 *****************************************************************************/
static SemaphoreHandle_t sdcard_mutex;
static SemaphoreHandle_t rtc_mutex;

/*****************************************************************************
 *                               INO FUNCTIONS                               *
 *****************************************************************************/
void debugMessage(const char *message, const char *logfile = "", DebugLevels debug_level = DebugLevels::Info) {
  const char *errorlevel = debugLevelName[debug_level];
  if (debug_level < debugLevel) {
    return;
  }

  char timeStr[10];
  char msgBuffer[512];
  clockRtc.getTimeString(timeStr);
  // Format the message
  sprintf(msgBuffer, "%s\t%s\t%s %s\t%d\t%d\t%d", timeStr, errorlevel, FILE_NAME, message, xPortGetCoreID(), rp2040.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));

  Log_Entry logEntry;
  strlcpy(logEntry.message, msgBuffer, sizeof(logEntry.message));
  strlcpy(logEntry.logfile, logfile, sizeof(logEntry.logfile));

  if (xQueueSend(log_queue, (void *)&logEntry, 10) != pdTRUE) {
    Dbg_printf("%s: %s debugMessage - Log Queue Full -----------------------------\n", debugLevelName[DebugLevels::Error], FILE_NAME);
    Dbg_printf("%s\n", msgBuffer);
  }
}

/*****************************************************************************
 *                                   TASKS                                   *
 *****************************************************************************/
void checkRemote(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  int loopCount = 0;
  char msgBuffer[255];

  UBaseType_t uxHighWaterMark;

  // Setup Indicator LED
  pinMode(PIN_LED_INDICATOR, OUTPUT);

  clockRemote.begin();

  while (1) {
    digitalWrite(PIN_LED_INDICATOR, LOW);

    char button = clockRemote.checkForButton();

    if (button != '\0') {
      if (xQueueSend(remote_queue, (void *)&button, 10) != pdTRUE) {
        debugMessage("checkRemote - Output Queue Full -----------------------------", logDebug, DebugLevels::Error);
      } else {
        digitalWrite(PIN_LED_INDICATOR, HIGH);
      }
    }

    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("checkRemote");
      loopCount = 0;
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

void checkSdCard(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  int loopCount = 0;
  Log_Entry item;

  while (1) {
    if (xSemaphoreTake(sdcard_mutex, 100) == pdTRUE) {
      if (!clockSdCard.isCardPresent()) {
        last_configLoadMillis = 0;
        configLoadMillis = 0;
      } else if (configLoadMillis == 0) {
        configLoadMillis = millis();
      }
      xSemaphoreGive(sdcard_mutex);
    } else {
      continue;
    }

    // Check if there are log entries to write to the SD Card
    bool writeToFile = false;
    while (xQueueReceive(log_queue, (void *)&item, 0) == pdTRUE) {
      if (configLoadMillis != 0 && strlen(item.logfile) > 0) {
        writeToFile = true;
      }

      if (writeToFile) {
        if (xSemaphoreTake(sdcard_mutex, 10) == pdTRUE) {
          clockSdCard.writeLogEntry(item.logfile, item.message);
          xSemaphoreGive(sdcard_mutex);
        } else {
          Dbg_printf("%s: %s checkSdCard - Failed to write log entry to the SD Card. (Could not obtain mutex.)", DebugLevels::Error, FILE_NAME);
          writeToFile = false;
        }
      }

      if (!writeToFile || debugLevel == DebugLevels::Verbose) {
        Dbg_printf("%s\n", item.message);
      }
    }

    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("checkSdCard");
      loopCount = 0;
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}


// void checkWiFi(void *param) {
void checkWiFi() {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  int loopCount = 0;
  char msgBuffer[255];

  while (1) {
    // If the SD Card was inserted/reinserted, use the new configuration to connect to the Wi-Fi AP
    if (configLoadMillis != last_configLoadMillis) {
      debugMessage("checkWiFi - START: Restart Wi-Fi with new Config", logDebug);

      unsigned long startConnect = millis();
      clockWifi.begin(&clockSdCard.sdCardConfig);
      if (clockWifi.hasIpAddress()) {
        sprintf(msgBuffer, "checkWiFi - Connected in %d ms", millis() - startConnect);
      } else {
        sprintf(msgBuffer, "checkWiFi - Failed to connect in %d ms", millis() - startConnect);
      }
      debugMessage(msgBuffer, logDebug);
      debugMessage("checkWiFi - Wi-Fi Begin Completed", logDebug);
      last_configLoadMillis = configLoadMillis;
      debugMessage("checkWiFi - END: Restart Wi-Fi with new Config", logDebug);
    }

    // Set the IP Address property
    bool hasIp = clockWifi.hasIpAddress();

    // If the IP Address has changed, print the IP Addess
    if (strcmp(last_ipaddress, clockWifi.ipAddress) != 0) {
      debugMessage("checkWiFi - START: Update IP Address", logDebug);
      strcpy(last_ipaddress, clockWifi.ipAddress);
      clockOutput.updateIpAddress(last_ipaddress);
      sprintf(msgBuffer, "checkWiFi - IP Address has changed to %s", last_ipaddress);
      debugMessage(msgBuffer, logDebug);
      debugMessage("checkWiFi - END: Update IP Address", logDebug);
    }

    // If the Wi-Fi Mode has changed, print debug message
    if (last_WiFiMode != clockWifi.wifiMode) {
      last_WiFiMode = clockWifi.wifiMode;
      sprintf(msgBuffer, "checkWiFi - Wi-Fi has changed to %s", wifiModeName[last_WiFiMode]);
      debugMessage(msgBuffer, logDebug);
    }

    char currentTime[10] = "--:--";
    if (hasIp && !clockRtc.timeIsSet()) {
      clockRtc.getInternetTime();
      if (clockRtc.timeIsSet()) {
        clockRtc.getTimeString(currentTime);

        sprintf(msgBuffer, "checkWiFi - Current time is: %s", currentTime);
        debugMessage(msgBuffer, logDebug);
      }
    }

    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("checkWiFi");
      loopCount = 0;
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

void clockUpdate(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  int loopCount = 0;
  char msgBuffer[255];

  char currentTime[10] = "--:--";
  char item;

  clockOutput.begin();

  while (1) {
    if (clockRtc.timeIsSet()) {
      clockRtc.getTimeString(currentTime);
    } else {
      strlcpy(currentTime, "--:--", sizeof(currentTime));
    }

    while (xQueueReceive(remote_queue, (void *)&item, 0) == pdTRUE) {
      // static constexpr char _buttons[17]{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'U', 'D', 'L', 'R', 'K' };
      switch (item) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          char numberBuffer[10];
          if (clockOutput.clockMode == ClockMode::TimerSetMin) {
            int minVal = clockOutput.timerSetMin();
            sprintf(numberBuffer, "%d%c", minVal, item);
            minVal = atoi(numberBuffer);
            minVal = clockOutput.timerSetMin(minVal);
          } else if (clockOutput.clockMode == ClockMode::TimerSetMax) {
            int maxVal = clockOutput.timerSetMax();
            sprintf(numberBuffer, "%d%c", maxVal, item);
            maxVal = atoi(numberBuffer);
            maxVal = clockOutput.timerSetMax(maxVal);
          }
          break;
        case '*':
          clockOutput.clockMode = ClockMode::Clock;
          break;
        case '#':
          if (clockOutput.clockMode == ClockMode::Clock) {
            clockOutput.clockMode = ClockMode::TimerReady;
            clockOutput.timerNext(false);
          } else if (clockOutput.clockMode == ClockMode::TimerReady || clockOutput.clockMode == ClockMode::TimerSetMin || clockOutput.clockMode == ClockMode::TimerSetMax) {
            clockOutput.timerNext();
          }
          break;
        case 'U':
          break;
        case 'D':
          break;
        case 'L':
          if(clockOutput.clockMode == ClockMode::TimerSetMax) {
            clockOutput.clockMode = ClockMode::TimerSetMin;
          }
          break;
        case 'R':
          if(clockOutput.clockMode == ClockMode::TimerSetMin) {
            clockOutput.clockMode = ClockMode::TimerSetMax;
          }
          break;
        case 'K':
          if(clockOutput.clockMode == ClockMode::TimerReady) {
            clockOutput.clockMode = ClockMode::TimerRun;
          } else if(clockOutput.clockMode == ClockMode::TimerRun ) {
            clockOutput.clockMode = ClockMode::TimerStop;
          } else if(clockOutput.clockMode == ClockMode::TimerStop ) {
            clockOutput.clockMode = ClockMode::TimerReady;
          }
          break;
        default:
          break;
      }
    }

    switch (clockOutput.clockMode) {
      case ClockMode::TimerReady:
      case ClockMode::TimerSetMin:
      case ClockMode::TimerSetMax:
      case ClockMode::TimerRun:
      case ClockMode::TimerStop:
        clockOutput.updateTimer();
        break;
      case ClockMode::TestColors:
        break;
      case ClockMode::TestRainbow:
        break;
      case ClockMode::Clock:
      default:
        clockOutput.updateTime(currentTime);
        break;
    }

    if (last_clockMode != clockOutput.clockMode) {
      sprintf(msgBuffer, "clockUpdate - Clock Mode has changed from %s to %s", debugClockModeName[last_clockMode], debugClockModeName[clockOutput.clockMode]);
      debugMessage(msgBuffer, logDebug);

      last_clockMode = clockOutput.clockMode;
    }



    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("clockUpdate");
      loopCount = 0;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/*****************************************************************************
 *                                   SETUP                                   *
 *****************************************************************************/
void setup() {
  Dbg_begin(115200);

  // Wait for the serial port to connect
  while (DEBUG && !(Serial || Serial1)) {
    delay(1);  // wait for serial port to connect.
  }

  // Print 5 blank lines on startup
  for (int i = 0; i < 5; i++) {
    Dbg_println("");
  }

  // START: SD Card
  clockSdCard.begin();
  // NOTE: The SD Card needs to be inserted with the Config.txt file or the code will
  // never attempt to connect to Wi-Fi. In the final version, there will be a task to
  // check the SD Card and reload the configuration file.
  if (clockSdCard.isCardPresent()) {
    configLoadMillis = millis();
    // *** Archive old backup files ***
    clockSdCard.renameFile(logDebug, logDebugArchive);
  }
  // END: SD Card

  // START: RTC
  clockRtc.begin();
  // END: RTC

  // Create Queue
  remote_queue = xQueueCreate(remote_queue_len, sizeof(char));
  log_queue = xQueueCreate(log_queue_len, sizeof(Log_Entry));

  // Create Mutex before starting tasks
  sdcard_mutex = xSemaphoreCreateMutex();
  rtc_mutex = xSemaphoreCreateMutex();

  debugMessage("setup -----------------------------");

  setup_complete = true;

  xTaskCreate(checkSdCard, "SDCARD", 2048, nullptr, 1, nullptr);
  xTaskCreate(checkRemote, "REMOTE", 2048, nullptr, 1, nullptr);
  // xTaskCreate(handleQueueItems, "HANDLE_QUEUE", 2048, nullptr, 1, nullptr);
  xTaskCreate(clockUpdate, "CLOCK", 2048, nullptr, 1, nullptr);
}

/*****************************************************************************
 *                                    LOOP                                   *
 *****************************************************************************/
void loop() {
  // The core written by Earle F. Philhower, III, requires that the Wi-Fi be
  // accessed from Core 0, therefore it needs to run in the loop.
  checkWiFi();
}
