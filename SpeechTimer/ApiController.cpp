#include "ApiController.h"
#include "Defines.h"
#include <cstdio>

#define FILE_NAME_API "[ApiController.cpp]"

ApiController::ApiController() 
  : _output(nullptr),
    _wifi(nullptr),
    _rtc(nullptr),
    _sdcard(nullptr),
    _rtc_mutex(nullptr),
    _wifi_state_mutex(nullptr),
    _display_state_mutex(nullptr),
    _config_mutex(nullptr),
    _initialized(false),
    _lastTestUpdate(0),
    _testStep(0) {
}

void ApiController::begin(Clk_Output* output, Clk_Wifi* wifi, Clk_Rtc* rtc, Clk_SdCard* sdcard) {
  _output = output;
  _wifi = wifi;
  _rtc = rtc;
  _sdcard = sdcard;
  _initialized = (_output != nullptr && _wifi != nullptr && _rtc != nullptr && _sdcard != nullptr);
  
  DBG_VERBOSE("%s begin - Initialized: %s\n", FILE_NAME_API, _initialized ? "true" : "false");
}

void ApiController::setMutexes(SemaphoreHandle_t rtc_mx, SemaphoreHandle_t wifi_mx, 
                               SemaphoreHandle_t display_mx, SemaphoreHandle_t config_mx) {
  _rtc_mutex = rtc_mx;
  _wifi_state_mutex = wifi_mx;
  _display_state_mutex = display_mx;
  _config_mutex = config_mx;
}

// === Helper Methods ===

bool ApiController::takeMutex(SemaphoreHandle_t mutex, TickType_t timeout) {
  if (mutex == nullptr) return false;
  return xSemaphoreTake(mutex, timeout) == pdTRUE;
}

void ApiController::giveMutex(SemaphoreHandle_t mutex) {
  if (mutex != nullptr) {
    xSemaphoreGive(mutex);
  }
}

bool ApiController::isTimeElapsed(unsigned long& lastTime, unsigned long interval) {
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    lastTime = currentTime;
    return true;
  }
  return false;
}

// === LED Control ===

bool ApiController::setLedState(bool on) {
  if (!_initialized) return false;
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
  
  DBG_VERBOSE("%s setLedState - LED: %s\n", FILE_NAME_API, on ? "ON" : "OFF");
  return true;
}

bool ApiController::getLedState() {
  return digitalRead(LED_BUILTIN);
}

// === Clock Display Control ===

bool ApiController::setClockColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (!_initialized || !takeMutex(_display_state_mutex)) return false;
  
  _output->setClockColor(red, green, blue);
  
  DBG_VERBOSE("%s setClockColor - RGB: %d, %d, %d\n", FILE_NAME_API, red, green, blue);
  
  giveMutex(_display_state_mutex);
  return true;
}

void ApiController::getClockColor(uint8_t& red, uint8_t& green, uint8_t& blue) {
  _output->getClockColor(red, green, blue);
}

// === Timer Control ===

bool ApiController::startTimer(int minMinutes, int maxMinutes) {
  if (!_initialized || !takeMutex(_display_state_mutex)) return false;
  
  // Validate and normalize
  if (minMinutes < 1) minMinutes = 1;
  if (maxMinutes < 1) maxMinutes = 1;
  if (minMinutes > maxMinutes) {
    int temp = minMinutes;
    minMinutes = maxMinutes;
    maxMinutes = temp;
  }
  
  // Set timer parameters in Clk_Output
  _output->timerSetMin(minMinutes);
  _output->timerSetMax(maxMinutes);
  
  // Switch to timer mode
  _output->clockMode = ClockMode::TimerRun;
  
  DBG_VERBOSE("%s startTimer - Min: %d, Max: %d\n", FILE_NAME_API, minMinutes, maxMinutes);
  
  giveMutex(_display_state_mutex);
  return true;
}

bool ApiController::stopTimer() {
  if (!_initialized || !takeMutex(_display_state_mutex)) return false;
  
  // Switch to timer stop mode, then back to clock
  _output->clockMode = ClockMode::TimerStop;
  
  DBG_VERBOSE("%s stopTimer\n", FILE_NAME_API);
  
  giveMutex(_display_state_mutex);
  return true;
}

bool ApiController::isTimerRunning() {
  if (!_initialized) return false;
  
  bool running = false;
  if (takeMutex(_display_state_mutex)) {
    running = (_output->clockMode == ClockMode::TimerRun);
    giveMutex(_display_state_mutex);
  }
  
  return running;
}

void ApiController::getTimerStatus(char* timeDisplay, uint8_t& red, uint8_t& green, uint8_t& blue, 
                                   int& minTime, int& maxTime, int& elapsedSeconds) {
  if (!_initialized || timeDisplay == nullptr) return;
  
  if (takeMutex(_display_state_mutex)) {
    // Get time display string from Clk_Output
    // You'll need to add a method to Clk_Output to get this
    strcpy(timeDisplay, "00:00"); // Placeholder
    
    // Get timer color (would need to be calculated based on elapsed time)
    red = 255;
    green = 255;
    blue = 255;
    
    // Get min/max times
    minTime = 5;  // Placeholder - need to store these in Clk_Output
    maxTime = 7;  // Placeholder
    
    // Get elapsed seconds
    elapsedSeconds = 0; // Placeholder - need to track start time
    
    giveMutex(_display_state_mutex);
  }
}

// === Mode Control ===

bool ApiController::setClockMode(ClockMode mode) {
  if (!_initialized || !takeMutex(_display_state_mutex)) return false;
  
  _output->clockMode = mode;
  
  DBG_VERBOSE("%s setClockMode - Mode: %s\n", FILE_NAME_API, getModeName(mode));
  
  giveMutex(_display_state_mutex);
  return true;
}

ClockMode ApiController::getCurrentMode() {
  if (!_initialized) return ClockMode::Clock;
  
  ClockMode mode = ClockMode::Clock;
  if (takeMutex(_display_state_mutex)) {
    mode = _output->clockMode;
    giveMutex(_display_state_mutex);
  }
  
  return mode;
}

const char* ApiController::getModeName(ClockMode mode) {
  switch (mode) {
    case ClockMode::Clock: return "CLOCK";
    case ClockMode::TimerReady: return "TIMER_READY";
    case ClockMode::TimerSetMin: return "TIMER_SET_MIN";
    case ClockMode::TimerSetMax: return "TIMER_SET_MAX";
    case ClockMode::TimerRun: return "TIMER_RUN";
    case ClockMode::TimerStop: return "TIMER_STOP";
    case ClockMode::TestColors: return "TEST_COLORS";
    case ClockMode::TestRainbow: return "TEST_RAINBOW";
    default: return "UNKNOWN";
  }
}

// === System Information ===

void ApiController::getSystemInfo(char* jsonBuffer, size_t bufferSize) {
  if (!_initialized || jsonBuffer == nullptr || bufferSize == 0) return;
  
  // Get system information
  float cpuTemp = analogReadTemp();
  uint32_t cpuFreq = rp2040.f_cpu();
  
  // Get WiFi info
  char ipAddr[16] = "0.0.0.0";
  if (takeMutex(_wifi_state_mutex)) {
    strncpy(ipAddr, _wifi->ipAddress, sizeof(ipAddr) - 1);
    giveMutex(_wifi_state_mutex);
  }
  
  // Format as JSON (simplified - you may want to use ArduinoJson library)
  snprintf(jsonBuffer, bufferSize,
    "{"
    "\"cpu\":{"
      "\"temperature_c\":%.1f,"
      "\"temperature_f\":%.1f,"
      "\"frequency\":%lu"
    "},"
    "\"wifi\":{"
      "\"ip_address\":\"%s\","
      "\"connected\":%s"
    "}"
    "}",
    cpuTemp,
    cpuTemp * 1.8 + 32,
    cpuFreq,
    ipAddr,
    _wifi->isWiFiConnected() ? "true" : "false"
  );
}

// === Time ===

void ApiController::getCurrentTime(char* timeString, size_t bufferSize) {
  if (!_initialized || timeString == nullptr || bufferSize == 0) return;
  
  if (takeMutex(_rtc_mutex)) {
    _rtc->getTimeString(timeString);
    giveMutex(_rtc_mutex);
  } else {
    strncpy(timeString, "--:--:--", bufferSize - 1);
  }
}

bool ApiController::isTimeSet() {
  if (!_initialized) return false;
  
  bool timeSet = false;
  if (takeMutex(_rtc_mutex)) {
    timeSet = _rtc->timeIsSet();
    giveMutex(_rtc_mutex);
  }
  
  return timeSet;
}

// === Test Mode ===

void ApiController::updateTestMode() {
  if (!_initialized) return;
  
  if (!isTimeElapsed(_lastTestUpdate, 1000)) return; // Update every 1 second
  
  if (takeMutex(_display_state_mutex)) {
    if (_output->clockMode != ClockMode::TestColors && _output->clockMode != ClockMode::TestRainbow) {
      giveMutex(_display_state_mutex);
      return;
    }
    
    // Cycle through colors
    if (_testStep > 6) _testStep = 0;
    
    // Update display with test pattern "88:88"
    _output->updateTime("88:88");
    
    // You would set different colors here based on testStep
    // This requires adding color control methods to Clk_Output
    
    DBG_VERBOSE("%s updateTestMode - Step: %d\n", FILE_NAME_API, _testStep);
    
    _testStep++;
    
    giveMutex(_display_state_mutex);
  }
}
