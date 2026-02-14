#ifndef API_CONTROLLER_H
#define API_CONTROLLER_H

#include <Arduino.h>
#include "Clk_Output.h"
#include "Clk_Wifi.h"
#include "Clk_Rtc.h"
#include "Clk_SdCard.h"
#include "StructsAndEnums.h"

/**
 * API Controller - Business Logic Layer
 * 
 * This class provides a transport-agnostic API for controlling the Speech Timer.
 * It can be used with WiFi (HTTP), Bluetooth, or any other communication protocol.
 * 
 * All methods work with the existing FreeRTOS task architecture and use mutexes
 * for thread-safe access to shared resources.
 * 
 * Architecture:
 *   UI Layer (Web/Mobile) 
 *       ↓
 *   Transport Layer (WebServer/Bluetooth)
 *       ↓
 *   API Controller (This class) ← Business Logic
 *       ↓
 *   Hardware Layer (Clk_Output, Clk_Wifi, etc.)
 */
class ApiController {
public:
  // Constructor
  ApiController();
  
  // Initialize with references to hardware modules
  void begin(Clk_Output* output, Clk_Wifi* wifi, Clk_Rtc* rtc, Clk_SdCard* sdcard);
  
  // Set mutex handles for thread-safe access
  void setMutexes(SemaphoreHandle_t rtc_mx, SemaphoreHandle_t wifi_mx, 
                  SemaphoreHandle_t display_mx, SemaphoreHandle_t config_mx);
  
  // === LED Control ===
  bool setLedState(bool on);
  bool getLedState();
  
  // === Clock Display Control ===
  bool setClockColor(uint8_t red, uint8_t green, uint8_t blue);
  void getClockColor(uint8_t& red, uint8_t& green, uint8_t& blue);
  
  // === Timer Control ===
  bool startTimer(int minMinutes, int maxMinutes);
  bool stopTimer();
  bool isTimerRunning();
  void getTimerStatus(char* timeDisplay, uint8_t& red, uint8_t& green, uint8_t& blue, 
                     int& minTime, int& maxTime, int& elapsedSeconds);
  
  // === Mode Control ===
  bool setClockMode(ClockMode mode);
  ClockMode getCurrentMode();
  const char* getModeName(ClockMode mode);
  
  // === System Information ===
  void getSystemInfo(char* jsonBuffer, size_t bufferSize);
  
  // === Time ===
  void getCurrentTime(char* timeString, size_t bufferSize);
  bool isTimeSet();
  
  // === Test Mode ===
  void updateTestMode();

private:
  // Hardware module pointers
  Clk_Output* _output;
  Clk_Wifi* _wifi;
  Clk_Rtc* _rtc;
  Clk_SdCard* _sdcard;
  
  // Mutex handles for thread-safe access
  SemaphoreHandle_t _rtc_mutex;
  SemaphoreHandle_t _wifi_state_mutex;
  SemaphoreHandle_t _display_state_mutex;
  SemaphoreHandle_t _config_mutex;
  
  // Internal state
  bool _initialized;
  unsigned long _lastTestUpdate;
  int _testStep;
  
  // Helper methods
  bool takeMutex(SemaphoreHandle_t mutex, TickType_t timeout = pdMS_TO_TICKS(100));
  void giveMutex(SemaphoreHandle_t mutex);
  
  // Prevent timer overflow issues
  bool isTimeElapsed(unsigned long& lastTime, unsigned long interval);
};

#endif // API_CONTROLLER_H
