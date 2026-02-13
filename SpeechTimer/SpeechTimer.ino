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

int waitLoopsToPrintTaskInfo = LOOPS_BEFORE_TASK_INFO_PRINT;

/*****************************************************************************
 *                                   QUEUES                                  *
 *****************************************************************************/
static const uint8_t log_queue_len = LOG_QUEUE_LENGTH;
static QueueHandle_t log_queue;

static const uint8_t remote_queue_len = REMOTE_QUEUE_LENGTH;
static QueueHandle_t remote_queue;

/*****************************************************************************
 *                                   MUTEX                                   *
 *****************************************************************************/
static SemaphoreHandle_t sdcard_mutex;
static SemaphoreHandle_t rtc_mutex;
static SemaphoreHandle_t config_mutex;
static SemaphoreHandle_t wifi_state_mutex;
static SemaphoreHandle_t display_state_mutex;

/*****************************************************************************
 *                               INO FUNCTIONS                               *
 *****************************************************************************/
void debugMessage(const char *message, const char *logfile = "", DebugLevels debug_level = DebugLevels::Info) {
  const char *errorlevel = debugLevelName[debug_level];
  if (debug_level < debugLevel) {
    return;
  }

  char timeStr[10] = "--:--";
  char msgBuffer[512];
  
  // Protect RTC access with mutex
  if (xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
    clockRtc.getTimeString(timeStr);
    xSemaphoreGive(rtc_mutex);
  }
  
  // Format the message
  sprintf(msgBuffer, "%s\t%s\t%s %s\t%d\t%d\t%d", timeStr, errorlevel, FILE_NAME, message, xPortGetCoreID(), rp2040.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));

  Log_Entry logEntry;
  strlcpy(logEntry.message, msgBuffer, sizeof(logEntry.message));
  strlcpy(logEntry.logfile, logfile, sizeof(logEntry.logfile));

  if (xQueueSend(log_queue, (void *)&logEntry, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
    // Queue is full - try to make room by dropping oldest entry
    Log_Entry dummy;
    if (xQueueReceive(log_queue, &dummy, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
      Dbg_printf("%s: %s debugMessage - Log Queue Full, dropped oldest entry\n", 
                 debugLevelName[DebugLevels::Warning], FILE_NAME);
      
      // Try to add new entry again
      if (xQueueSend(log_queue, (void *)&logEntry, QUEUE_RECEIVE_NO_WAIT) != pdTRUE) {
        // Still failed - critical error
        Dbg_printf("%s: %s debugMessage - CRITICAL: Failed to queue after making room:\n%s\n", 
                   debugLevelName[DebugLevels::Error], FILE_NAME, msgBuffer);
      }
    } else {
      // Couldn't even remove an entry - critical error
      Dbg_printf("%s: %s debugMessage - CRITICAL: Log Queue Full and cannot remove entries:\n%s\n", 
                 debugLevelName[DebugLevels::Error], FILE_NAME, msgBuffer);
    }
  }
}

/*****************************************************************************
 *                                   TASKS                                   *
 *****************************************************************************/
void checkRemote(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_SETUP_WAIT));
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
      if (xQueueSend(remote_queue, (void *)&button, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        // Queue is full - try to make room by dropping oldest button press
        char dummy;
        if (xQueueReceive(remote_queue, &dummy, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
          // Successfully removed oldest, try to add new button
          if (xQueueSend(remote_queue, (void *)&button, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
            digitalWrite(PIN_LED_INDICATOR, HIGH);
            debugMessage("checkRemote - Remote Queue Full, dropped oldest button", logDebug, DebugLevels::Warning);
          } else {
            debugMessage("checkRemote - CRITICAL: Remote Queue Full, button press lost", logDebug, DebugLevels::Error);
          }
        } else {
          debugMessage("checkRemote - CRITICAL: Remote Queue Full and cannot remove entries", logDebug, DebugLevels::Error);
        }
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
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_REMOTE_CHECK));
  }
}

void checkSdCard(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_SETUP_WAIT));
  }

  int loopCount = 0;
  Log_Entry item;

  while (1) {
    bool cardPresent = false;
    unsigned long currentConfigLoadMillis = 0;
    
    if (xSemaphoreTake(sdcard_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      cardPresent = clockSdCard.isCardPresent();
      xSemaphoreGive(sdcard_mutex);
    } else {
      continue;
    }
    
    // Protect config state variables with mutex
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      if (!cardPresent) {
        last_configLoadMillis = 0;
        configLoadMillis = 0;
      } else if (configLoadMillis == 0) {
        configLoadMillis = millis();
      }
      currentConfigLoadMillis = configLoadMillis;
      xSemaphoreGive(config_mutex);
    } else {
      continue;
    }

    // Check if there are log entries to write to the SD Card
    bool writeToFile = false;
    while (xQueueReceive(log_queue, (void *)&item, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
      if (currentConfigLoadMillis != 0 && strlen(item.logfile) > 0) {
        writeToFile = true;
      }

      if (writeToFile) {
        // Retry logic to prevent log entry loss
        constexpr uint8_t MAX_RETRIES = MAX_SDCARD_WRITE_RETRIES;
        uint8_t retries = 0;
        bool writtenSuccessfully = false;
        
        while (retries < MAX_RETRIES) {
          if (xSemaphoreTake(sdcard_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_SHORT_MS)) == pdTRUE) {
            clockSdCard.writeLogEntry(item.logfile, item.message);
            xSemaphoreGive(sdcard_mutex);
            writtenSuccessfully = true;
            break;
          }
          retries++;
          if (retries < MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));  // Brief delay before retry
          }
        }
        
        if (!writtenSuccessfully) {
          // Failed after all retries - try to preserve the message by re-queuing
          Dbg_printf("%s: %s checkSdCard - Failed to write log entry after %d retries. Attempting to re-queue.\n", 
                     debugLevelName[DebugLevels::Error], FILE_NAME, MAX_RETRIES);
          
          // Try to put the item back at the front of the queue
          if (xQueueSendToFront(log_queue, (void *)&item, QUEUE_RECEIVE_NO_WAIT) != pdTRUE) {
            Dbg_printf("%s: %s checkSdCard - CRITICAL: Could not re-queue log entry. Message lost:\n%s\n", 
                       debugLevelName[DebugLevels::Error], FILE_NAME, item.message);
          }
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
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_SDCARD_CHECK));
  }
}


// void checkWiFi(void *param) {
void checkWiFi() {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_SETUP_WAIT));
  }

  int loopCount = 0;
  char msgBuffer[255];

  while (1) {
    // Protect config state variables with mutex
    unsigned long currentConfigLoadMillis = 0;
    unsigned long currentLastConfigLoadMillis = 0;
    
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      currentConfigLoadMillis = configLoadMillis;
      currentLastConfigLoadMillis = last_configLoadMillis;
      xSemaphoreGive(config_mutex);
    }
    
    // If the SD Card was inserted/reinserted, use the new configuration to connect to the Wi-Fi AP
    if (currentConfigLoadMillis != currentLastConfigLoadMillis) {
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
      
      // Update last_configLoadMillis with mutex protection
      if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
        last_configLoadMillis = configLoadMillis;
        xSemaphoreGive(config_mutex);
      }
      
      debugMessage("checkWiFi - END: Restart Wi-Fi with new Config", logDebug);
    }

    // Set the IP Address property
    bool hasIp = clockWifi.hasIpAddress();

    // Protect WiFi state variables with mutex
    if (xSemaphoreTake(wifi_state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      // If the IP Address has changed, print the IP Address
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
      
      xSemaphoreGive(wifi_state_mutex);
    }

    char currentTime[10] = "--:--";
    
    // Protect RTC access with mutex
    if (xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      if (hasIp && !clockRtc.timeIsSet()) {
        clockRtc.getInternetTime();
        if (clockRtc.timeIsSet()) {
          clockRtc.getTimeString(currentTime);

          sprintf(msgBuffer, "checkWiFi - Current time is: %s", currentTime);
          debugMessage(msgBuffer, logDebug);
        }
      }
      xSemaphoreGive(rtc_mutex);
    }

    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("checkWiFi");
      loopCount = 0;
    }
    
    // Adaptive delay: Fast when connecting, slow when stable
    // This improves responsiveness during connection while saving CPU when stable
    TickType_t delay = hasIp ? 
                       pdMS_TO_TICKS(TASK_DELAY_WIFI_CHECK_STABLE) :   // 1000ms when connected
                       pdMS_TO_TICKS(TASK_DELAY_WIFI_CHECK_UNSTABLE); // 250ms when connecting
    vTaskDelay(delay);
  }
}

/*****************************************************************************
 *                           REMOTE INPUT HANDLERS                           *
 *****************************************************************************/
void handleTimerDigitInput(char digit) {
  // Optimize: Use direct arithmetic instead of sprintf+atoi
  // Shift digits left and append new digit: 12 + '3' = 123
  int digitValue = digit - '0';  // Convert char to int
  
  if (clockOutput.clockMode == ClockMode::TimerSetMin) {
    int minVal = clockOutput.timerSetMin();
    minVal = (minVal * 10 + digitValue) % 100;  // Append digit, keep in valid range
    clockOutput.timerSetMin(minVal);
  } else if (clockOutput.clockMode == ClockMode::TimerSetMax) {
    int maxVal = clockOutput.timerSetMax();
    maxVal = (maxVal * 10 + digitValue) % 100;  // Append digit, keep in valid range
    clockOutput.timerSetMax(maxVal);
  }
}

void handleHashButton() {
  if (clockOutput.clockMode == ClockMode::Clock) {
    clockOutput.clockMode = ClockMode::TimerReady;
    clockOutput.timerNext(false);
  } else if (clockOutput.clockMode == ClockMode::TimerReady || 
             clockOutput.clockMode == ClockMode::TimerSetMin || 
             clockOutput.clockMode == ClockMode::TimerSetMax) {
    clockOutput.timerNext();
  }
}

void handleNavigationInput(char direction) {
  if (direction == 'L' && clockOutput.clockMode == ClockMode::TimerSetMax) {
    clockOutput.clockMode = ClockMode::TimerSetMin;
  } else if (direction == 'R' && clockOutput.clockMode == ClockMode::TimerSetMin) {
    clockOutput.clockMode = ClockMode::TimerSetMax;
  }
}

void handleOkButton() {
  if (clockOutput.clockMode == ClockMode::TimerReady) {
    clockOutput.clockMode = ClockMode::TimerRun;
  } else if (clockOutput.clockMode == ClockMode::TimerRun) {
    clockOutput.clockMode = ClockMode::TimerStop;
  } else if (clockOutput.clockMode == ClockMode::TimerStop) {
    clockOutput.clockMode = ClockMode::TimerReady;
  }
}

void clockUpdate(void *param) {
  // Make certain that setup has completed
  while (!setup_complete) {
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_SETUP_WAIT));
  }

  int loopCount = 0;
  char msgBuffer[255];

  char currentTime[10] = "--:--";
  char item;

  clockOutput.begin();

  while (1) {
    // Protect RTC access with mutex
    if (xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      if (clockRtc.timeIsSet()) {
        clockRtc.getTimeString(currentTime);
      } else {
        strlcpy(currentTime, "--:--", sizeof(currentTime));
      }
      xSemaphoreGive(rtc_mutex);
    } else {
      strlcpy(currentTime, "--:--", sizeof(currentTime));
    }

    while (xQueueReceive(remote_queue, (void *)&item, QUEUE_RECEIVE_NO_WAIT) == pdTRUE) {
      // Process remote control input
      if (item >= '0' && item <= '9') {
        handleTimerDigitInput(item);
      } else if (item == '*') {
        clockOutput.clockMode = ClockMode::Clock;
      } else if (item == '#') {
        handleHashButton();
      } else if (item == 'L' || item == 'R') {
        handleNavigationInput(item);
      } else if (item == 'K') {
        handleOkButton();
      }
      // Note: 'U' and 'D' buttons currently have no implementation
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

    // Protect display state variables with mutex
    if (xSemaphoreTake(display_state_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
      if (last_clockMode != clockOutput.clockMode) {
        sprintf(msgBuffer, "clockUpdate - Clock Mode has changed from %s to %s", debugClockModeName[last_clockMode], debugClockModeName[clockOutput.clockMode]);
        debugMessage(msgBuffer, logDebug);

        last_clockMode = clockOutput.clockMode;
      }
      xSemaphoreGive(display_state_mutex);
    }



    loopCount++;
    // Print message with stack usage once every ten loops
    if (loopCount > waitLoopsToPrintTaskInfo) {
      debugMessage("clockUpdate");
      loopCount = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CLOCK_UPDATE));
  }
}

/*****************************************************************************
 *                                   SETUP                                   *
 *****************************************************************************/
void setup() {
  Dbg_begin(115200);

  // Wait for the serial port to connect
#if DEBUG
  #if USE_PICOPROBE
    while (!Serial1) { 
      delay(1);  // wait for serial port to connect
    }
  #else
    while (!Serial) { 
      delay(1);  // wait for serial port to connect
    }
  #endif
#endif

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
    // Set initial config load time (protected before tasks start, but good practice)
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
  config_mutex = xSemaphoreCreateMutex();
  wifi_state_mutex = xSemaphoreCreateMutex();
  display_state_mutex = xSemaphoreCreateMutex();

  debugMessage("setup -----------------------------");

  setup_complete = true;

  xTaskCreate(checkSdCard, "SDCARD", TASK_STACK_SIZE, nullptr, 1, nullptr);
  xTaskCreate(checkRemote, "REMOTE", TASK_STACK_SIZE, nullptr, 1, nullptr);
  // xTaskCreate(handleQueueItems, "HANDLE_QUEUE", TASK_STACK_SIZE, nullptr, 1, nullptr);
  xTaskCreate(clockUpdate, "CLOCK", TASK_STACK_SIZE, nullptr, 1, nullptr);
}

/*****************************************************************************
 *                                    LOOP                                   *
 *****************************************************************************/
void loop() {
  // The core written by Earle F. Philhower, III, requires that the Wi-Fi be
  // accessed from Core 0, therefore it needs to run in the loop.
  checkWiFi();
}
