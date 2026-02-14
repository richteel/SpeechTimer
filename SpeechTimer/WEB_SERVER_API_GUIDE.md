# Speech Timer - Web Server & API Architecture

## Overview

The Speech Timer C++ code now includes a **layered architecture** that separates the UI layer from the business logic layer. This makes it easy to:
- Add multiple user interfaces (Web, Mobile, Bluetooth)
- Reuse the same business logic across all interfaces
- Maintain and test code independently

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                       UI LAYER                               │
│  ┌───────────────┐  ┌───────────────┐  ┌──────────────┐    │
│  │   Web UI      │  │  iOS App      │  │ Android App  │    │
│  │  (HTML/CSS)   │  │  (Future)     │  │  (Future)    │    │
│  └───────────────┘  └───────────────┘  └──────────────┘    │
└──────────────┬────────────────┬────────────────┬────────────┘
               │                │                │
┌──────────────▼────────────────▼────────────────▼────────────┐
│                    TRANSPORT LAYER                           │
│  ┌──────────────────┐         ┌──────────────────┐          │
│  │  WebServer.cpp   │         │  Bluetooth       │          │
│  │  (Implemented)   │         │  (Future)        │          │
│  └──────────────────┘         └──────────────────┘          │
└──────────────┬─────────────────────────┬─────────────────────┘
               │                         │
┌──────────────▼─────────────────────────▼─────────────────────┐
│               BUSINESS LOGIC LAYER                            │
│                 ApiController.cpp                             │
│                                                               │
│  setLedState() | setClockColor() | startTimer()              │
│  stopTimer() | setClockMode() | getSystemInfo()              │
│  getCurrentTime() | isTimeSet() | updateTestMode()           │
└──────────────┬────────────────────────────────────────────────┘
               │
┌──────────────▼────────────────────────────────────────────────┐
│                    HARDWARE LAYER                             │
│  Clk_Output | Clk_Wifi | Clk_Rtc | Clk_SdCard | Clk_Remote   │
└───────────────────────────────────────────────────────────────┘
```

## New Files

### 1. ApiController.h / ApiController.cpp
**Business Logic Layer** - Transport-agnostic API for device control

**Key Features:**
- Thread-safe access using FreeRTOS mutexes
- No dependencies on HTTP, HTML, or any specific protocol
- Returns simple data types (bool, int, char arrays)
- Easy to call from any transport (WiFi, Bluetooth, Serial)

**Main Methods:**
```cpp
// LED Control
bool setLedState(bool on);
bool getLedState();

// Display Color Control  
bool setClockColor(uint8_t red, uint8_t green, uint8_t blue);
void getClockColor(uint8_t& red, uint8_t& green, uint8_t& blue);

// Timer Control
bool startTimer(int minMinutes, int maxMinutes);
bool stopTimer();
bool isTimerRunning();

// Mode Control
bool setClockMode(ClockMode mode);
ClockMode getCurrentMode();

// System Info
void getSystemInfo(char* jsonBuffer, size_t bufferSize);
void getCurrentTime(char* timeString, size_t bufferSize);
bool isTimeSet();

// Test Mode
void updateTestMode();
```

### 2. WebServer.h / WebServer.cpp
**Transport Layer** - HTTP server for web interface

**Key Features:**
- Handles HTTP GET and POST requests
- Parses form data from web UI
- Translates HTTP requests to API controller calls
- Serves JSON API endpoints

**Routes:**
- `GET /` - Main web interface
- `POST /` - Form submissions
- `GET /api/status` - JSON status endpoint
- `GET /api/system` - JSON system info
- `GET /api/time` - JSON time info

### 3. Modified SpeechTimer.ino
**Integration** - Connects everything together

**Changes:**
- Added includes for ApiController and WebServer
- Created global objects for apiController and webServer
- Initialized API controller with hardware module references
- Added `handleWebServer` FreeRTOS task
- Web server automatically starts/stops with WiFi connection

## How It Works

### Web Request Flow

```
1. User clicks button on web page
   ↓
2. Browser sends HTTP POST to WebServer
   ↓
3. WebServer parses form data
   ↓
4. WebServer calls ApiController method
     e.g., apiController.startTimer(5, 7)
   ↓
5. ApiController uses mutexes for thread-safe access
   ↓
6. ApiController calls hardware methods
     e.g., clockOutput.timerSetMin(), clockOutput.clockMode = Timer
   ↓
7. WebServer generates response HTML
   ↓
8. Browser displays updated page
```

### API Request Flow (JSON)

```
1. Mobile app/script makes GET request to /api/status
   ↓
2. WebServer routes to handleApiStatus()
   ↓
3. WebServer calls multiple ApiController methods
     - getCurrentMode()
     - isTimerRunning()
     - getCurrentTime()
     - getLedState()
   ↓
4. WebServer formats data as JSON
   ↓
5. Client receives JSON response
   {
     "mode": "CLOCK",
     "timer_running": false,
     "time": "14:30:45",
     "led": "off",
     "time_set": true
   }
```

## Using the Web Interface

### 1. Connect to WiFi
The device automatically connects to configured WiFi networks from config.txt on SD card.

### 2. Access Web Interface
Open browser and navigate to the device's IP address:
```
http://192.168.1.XXX/
```

### 3. Web Features

**LED Control**
- Click "LED ON" or "LED OFF" buttons

**Speech Timer**
- Set Min Time (minutes)
- Set Max Time (minutes)
- Click "START Timer" or "STOP Timer"

**Clock Mode**
- Set RGB color values (0-255)
- Click "Set Clock Color"

**Test Mode**
- Click "Start Test" to cycle through colors

## Using the JSON API

### Get Current Status
```bash
curl http://192.168.1.XXX/api/status
```

Response:
```json
{
  "mode": "CLOCK",
  "timer_running": false,
  "time": "14:30:45",
  "led": "off",
  "time_set": true
}
```

### Get System Information
```bash
curl http://192.168.1.XXX/api/system
```

Response:
```json
{
  "cpu": {
    "temperature_c": 32.5,
    "temperature_f": 90.5,
    "frequency": 125000000
  },
  "wifi": {
    "ip_address": "192.168.1.100",
    "connected": true
  }
}
```

### Get Current Time
```bash
curl http://192.168.1.XXX/api/time
```

Response:
```json
{
  "time": "14:30:45",
  "set": true
}
```

## Adding Bluetooth Support

To add Bluetooth, create a new transport layer class:

### 1. Create BluetoothServer.h / BluetoothServer.cpp

```cpp
#include "ApiController.h"
#include <BluetoothSerial.h>  // Or your BLE library

class BluetoothServer {
public:
  void begin(ApiController* apiController);
  void handleClient();
  
private:
  ApiController* _apiController;
  BluetoothSerial _bluetooth;
  
  void handleMessage(const char* message);
};
```

### 2. Implement Message Handler

```cpp
void BluetoothServer::handleMessage(const char* message) {
  // Parse JSON message from mobile app
  // Example: {"action": "start_timer", "min": 5, "max": 7}
  
  if (strcmp(action, "start_timer") == 0) {
    _apiController->startTimer(minTime, maxTime);
  } else if (strcmp(action, "stop_timer") == 0) {
    _apiController->stopTimer();
  } else if (strcmp(action, "set_color") == 0) {
    _apiController->setClockColor(red, green, blue);
  } else if (strcmp(action, "get_status") == 0) {
    // Build status JSON
    char json[512];
    snprintf(json, sizeof(json), 
      "{\"mode\":\"%s\",\"timer_running\":%s}",
      _apiController->getModeName(_apiController->getCurrentMode()),
      _apiController->isTimerRunning() ? "true" : "false"
    );
    // Send response via Bluetooth
  }
}
```

### 3. Add to main .ino

```cpp
BluetoothServer bluetoothServer;

void setup() {
  // ... existing setup ...
  bluetoothServer.begin(&apiController);
  
  xTaskCreate(handleBluetooth, "BLUETOOTH", TASK_STACK_SIZE, nullptr, 1, nullptr);
}

void handleBluetooth(void *param) {
  while (1) {
    bluetoothServer.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
```

## Mobile App Development

### iOS/Android Apps

Your mobile apps can communicate via:

**Option 1: WiFi (HTTP)** - Use the same API endpoints
```swift
// Swift example
func startTimer(min: Int, max: Int) {
    let url = URL(string: "http://192.168.1.100/")!
    var request = URLRequest(url: url)
    request.httpMethod = "POST"
    request.httpBody = "TIMER=START&MIN_TIME=\(min)&MAX_TIME=\(max)".data(using: .utf8)
    
    URLSession.shared.dataTask(with: request).resume()
}

func getStatus() async -> Status {
    let url = URL(string: "http://192.168.1.100/api/status")!
    let (data, _) = try await URLSession.shared.data(from: url)
    return try JSONDecoder().decode(Status.self, from: data)
}
```

**Option 2: Bluetooth (Future)** - Send JSON commands
```swift
// Swift BLE example
func startTimer(min: Int, max: Int) {
    let command = [
        "action": "start_timer",
        "min": min,
        "max": max
    ]
    let json = try! JSONEncoder().encode(command)
    peripheral.writeValue(json, for: characteristic)
}
```

## Benefits

### 1. Separation of Concerns
- **UI Layer**: Only handles presentation
- **Transport Layer**: Only handles protocol (HTTP, Bluetooth, etc.)
- **Business Logic Layer**: Only handles device behavior
- **Hardware Layer**: Only handles physical interfaces

### 2. Code ReuseTown
**Same API call works everywhere:**
```cpp
// From Web:
apiController.startTimer(5, 7);

// From Bluetooth (future):
apiController.startTimer(5, 7);

// From Serial (future):
apiController.startTimer(5, 7);
```

### 3. Thread Safety
All API methods use FreeRTOS mutexes for safe multi-task access.

### 4. Testability
Business logic can be tested without network connectivity:
```cpp
// Unit test example
ApiController api;
api.begin(&output, &wifi, &rtc, &sdcard);
bool result = api.startTimer(5, 7);
assert(result == true);
assert(api.isTimerRunning() == true);
```

### 5. Maintainability
Changes in one layer don't affect others:
- Change HTML → Only affects WebServer::generateMainPage()
- Change WiFi library → Only affects WebServer transport
- Change timer logic → Only affects ApiController
- Add new hardware → Only affects hardware layer

## Troubleshooting

### Web interface not accessible

1. Check IP address on OLED display
2. Verify WiFi connection: Check serial output for "WIFI: Starting Station Mode"
3. Check firewall/network settings

### API returning errors

1. Enable verbose debugging: `DebugLevels debugLevel = DebugLevels::Verbose;`
2. Check serial output for mutex timeouts
3. Verify hardware modules are initialized

### Compilation errors

1. Ensure all new files are in SpeechTimer folder
2. Check Arduino IDE has selected "Raspberry Pi Pico W"
3. Install WiFi library for Pico W if needed

## Future Enhancements

1. **Bluetooth Server** - BLE transport layer for mobile apps
2. **WebSocket Support** - Real-time updates without polling
3. **REST API** - Full RESTful endpoints (POST /api/timer, DELETE /api/timer, etc.)
4. **Authentication** - Basic auth or token-based security
5. **Configuration API** - Read/write config.txt via API
6. **Timer Presets** - Save/load timer configurations
7. **History** - Track timer sessions
8. **Mobile Apps** - Native iOS/Android apps

## Summary

✅ **Business logic** separated from transport layer
✅ **Web server** with HTML interface
✅ **JSON API** for programmatic access
✅ **Thread-safe** using FreeRTOS mutexes
✅ **Extensible** - easy to add Bluetooth, mobile apps, etc.
✅ **Reusable** - same API for all interfaces
✅ **Maintainable** - clear separation of concerns

The architecture is now ready for WiFi control and can easily be extended to Bluetooth and mobile apps!
