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
  - 
*/

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

#ifndef mainRUN_FREE_RTOS_ON_CORE
#define mainRUN_FREE_RTOS_ON_CORE 0
#endif

#include "DebugPrint.h"
#include <Dictionary.h>
#include "Clk_Output.h"
#include "Clk_Remote.h"
#include "Clk_SdCard.h"

// #if CONFIG_FREERTOS_UNICORE
// static const BaseType_t app_cpu = 0;
// #else
// static const BaseType_t app_cpu = 1;
// #endif

unsigned long previousMillis0 = 0;
unsigned long previousMillis1 = 0;
const long interval0 = 1000;  // interval to execute loop (Core 0) in milliseconds
const long interval1 = 1000;  // interval to execute loop (Core 1) in milliseconds

bool core0_setup_complete = false;

/**************** QUEUE HANDLER ***********************/
static const uint8_t output_queue_len = 5;
static QueueHandle_t output_queue;

static const uint8_t log_queue_len = 5;
static QueueHandle_t log_queue;

/**************** QUEUE HANDLER ***********************/
static SemaphoreHandle_t sdcard_mutex;
static SemaphoreHandle_t rtc_mutex;



/**************** HARDWARE OBJECTS ***********************/
Clk_Output clockOutput = Clk_Output();
Clk_SdCard clockSdCard = Clk_SdCard();

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

void setup() {
  // if (USE_PICOPROBE && DEBUG) {
  //   delay(6000);
  // }

  D_SerialBegin(115200);
  while (DEBUG && !(Serial || Serial1)) {
    delay(1);  // wait for serial port to connect.
  }

  debugMessage("setup");

  // Call begin for the SD Card before Output begin or the following error will occur.
  /*
    isr_hardfault@0x100030c4
    <signal handler called>@0xfffffffd
    ??@0x600143c8
    fs::FS::begin@0x1000abf2
    FS.cpp329:0
    SDClass::begin@0x10003c12
    SD.h40:0
    Clk_SdCard::begin@0x10003c12
    Clk_SdCard.cpp22:0
    setup@0x10003e82
    SpeechTimer_RTOS.ino164:0
    __core0@0x10006018
    variantHooks.cpp120:0
    vPortStartFirstTask@0x100063c8
    port.c208:0
  */

  D_println("START: Initialize Clock");
  clockOutput.begin();
  D_println("FINISH: Initialize Clock");

  D_println("START: Initialize SD Card");
  bool sdInit = clockSdCard.begin();
  D_println("FINISH: Initialize SD Card");
  D_printf("SD Card Initialized = %d\n", sdInit);

  // Create Queue
  output_queue = xQueueCreate(output_queue_len, sizeof(char));
  log_queue = xQueueCreate(log_queue_len, sizeof(char[255]));

  // Create Mutex before starting tasks
  sdcard_mutex = xSemaphoreCreateMutex();
  rtc_mutex = xSemaphoreCreateMutex();

  // Test NeoPixels
  // clockOutput.neopixelColorWipe(clockOutput.Color(255, 0, 255), 50);
  // clockOutput.neopixelRainbow(50);
  // clockOutput.neopixelTheaterChase(clockOutput.Color(255, 0, 255), 50);
  // clockOutput.neopixelTheaterChaseRainbow(50);

  clockOutput.updateIpAddress("127.0.0.1");
  clockOutput.updateTime("12:34");
  clockOutput.updateIpAddress("");

  core0_setup_complete = true;
}

void setup1() {
  while (!core0_setup_complete) {
    delay(1);
  }
  while (DEBUG && !(Serial || Serial1)) {
    delay(1);  // wait for serial port to connect.
  }

  debugMessage("setup1");

  xTaskCreate(checkRemote, "REMOTE", 192, nullptr, 1, nullptr);
  xTaskCreate(handleQueueItems, "HANDLE_QUEUE", 192, nullptr, 1, nullptr);
}

void loop() {
  // unsigned long currentMillis = millis();

  // if (currentMillis - previousMillis0 >= interval0) {
  //   previousMillis0 = currentMillis;
  //   debugMessage("loop");
  // }
}

void handleQueueItems(void *param) {
  char item;

  while (1) {

    // See if there's a message in the queue (do not block)
    if (xQueueReceive(output_queue, (void *)&item, 0) == pdTRUE) {
      D_printf("BUTTON PRESSED (handleQueueItems): %c\n", item);
    }
    debugMessage("handleQueueItems");
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void checkRemote(void *param) {
  (void)param;
  UBaseType_t uxHighWaterMark;

  // Setup Indicator LED
  pinMode(PIN_LED_INDICATOR, OUTPUT);

  Clk_Remote remote = Clk_Remote();
  remote.begin();

  while (1) {
    digitalWrite(PIN_LED_INDICATOR, LOW);

    char button = remote.checkForButton();

    if (button != '\0') {
      if (xQueueSend(output_queue, (void *)&button, 10) != pdTRUE) {
        D_println(F("ERROR: Output Queue Full -----------------------------"));
      } else {
        digitalWrite(PIN_LED_INDICATOR, HIGH);
      }
    }
    debugMessage("remote");
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void debugMessage(const char *message) {
  // D_printf("%s\t%d\t%d\t%d\n", message, xPortGetCoreID(), rp2040.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));
}
