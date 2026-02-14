#include "WebServer.h"
#include "Defines.h"
#include "Clk_SdCard.h"
#include <cstring>
#include <cstdlib>

#define FILE_NAME_WEB "[WebServer.cpp]"
#define REQUEST_BUFFER_SIZE 2048
#define RESPONSE_BUFFER_SIZE 2048

WebServer::WebServer(uint16_t port) 
  : _server(port),
    _apiController(nullptr),
    _sdcard(nullptr),
    _initialized(false),
    _port(port),
    _requestCount(0) {
}

void WebServer::begin(ApiController* apiController, Clk_SdCard* sdcard) {
  _apiController = apiController;
  _sdcard = sdcard;
  _initialized = (_apiController != nullptr && _sdcard != nullptr);
  
  if (_initialized) {
    _server.begin();
    DEBUGV("%s begin - Server started on port %d\n", FILE_NAME_WEB, _port);
  } else {
    DEBUGV("%s begin - Failed to initialize (null pointers)\n", FILE_NAME_WEB);
  }
}

void WebServer::stop() {
  _server.stop();
  _initialized = false;
  DEBUGV("%s stop - Server stopped\n", FILE_NAME_WEB);
}

bool WebServer::isRunning() {
  return _initialized;
}

// === Main Request Handler ===

void WebServer::handleClient() {
  if (!_initialized) return;
  
  WiFiClient client = _server.accept();
  if (!client) return;
  
  DEBUGV("%s handleClient - New client connected\n", FILE_NAME_WEB);
  _requestCount++;
  
  // Wait for data with timeout
  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) {
    delay(1);
  }
  
  if (client.available()) {
    handleRequest(client);
  } else {
    DEBUGV("%s handleClient - Timeout waiting for data\n", FILE_NAME_WEB);
  }
  
  client.stop();
  DEBUGV("%s handleClient - Client disconnected\n", FILE_NAME_WEB);
}

void WebServer::handleRequest(WiFiClient& client) {
  char method[16] = {0};
  char path[128] = {0};
  char body[512] = {0};
  
  if (!parseHttpRequest(client, method, path, body, sizeof(body))) {
    DEBUGV("%s handleRequest - Failed to parse request\n", FILE_NAME_WEB);
    handle404(client);
    return;
  }
  
  DEBUGV("%s handleRequest - %s %s\n", FILE_NAME_WEB, method, path);
  
  // Route the request
  if (strcmp(method, "GET") == 0) {
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
      handleGetRoot(client);
    } else if (strcmp(path, "/api/dashboard") == 0) {
      handleApiDashboard(client);
    } else if (strcmp(path, "/api/status") == 0) {
      handleApiStatus(client);
    } else if (strcmp(path, "/api/system") == 0) {
      handleApiSystem(client);
    } else if (strcmp(path, "/api/time") == 0) {
      handleApiTime(client);
    } else if (strcmp(path, "/css/dashboard.css") == 0) {
      serveFile(client, "wwwroot/css/dashboard.css", "text/css");
    } else if (strcmp(path, "/js/dashboard.js") == 0) {
      serveFile(client, "wwwroot/js/dashboard.js", "application/javascript");
    } else {
      handle404(client);
    }
  } else if (strcmp(method, "POST") == 0) {
    if (strcmp(path, "/") == 0) {
      handlePostRoot(client, body);
    } else {
      handle404(client);
    }
  } else {
    handle404(client);
  }
}

// === Route Handlers ===

void WebServer::handleGetRoot(WiFiClient& client) {
  // Serve static index.html from SD card
  serveFile(client, "wwwroot/index.html", "text/html");
}

void WebServer::handlePostRoot(WiFiClient& client, const char* body) {
  // Parse form data
  char params[10][32];
  char values[10][64];
  int paramCount = 0;
  
  parseFormData(body, params, values, paramCount, 10);
  
  DEBUGV("%s handlePostRoot - Parsed %d parameters\n", FILE_NAME_WEB, paramCount);
  
  // Process form data
  char value[64];
  
  // LED control
  if (getUrlParamValue(params, values, paramCount, "LED", value, sizeof(value)) >= 0) {
    if (strcmp(value, "ON") == 0) {
      _apiController->setLedState(true);
    } else if (strcmp(value, "OFF") == 0) {
      _apiController->setLedState(false);
    }
  }
  
  // Clock color
  if (getUrlParamValue(params, values, paramCount, "CLOCK", value, sizeof(value)) >= 0) {
    uint8_t red = 255, green = 255, blue = 255;
    
    char colorVal[16];
    if (getUrlParamValue(params, values, paramCount, "RED", colorVal, sizeof(colorVal)) >= 0) {
      red = atoi(colorVal);
    }
    if (getUrlParamValue(params, values, paramCount, "GREEN", colorVal, sizeof(colorVal)) >= 0) {
      green = atoi(colorVal);
    }
    if (getUrlParamValue(params, values, paramCount, "BLUE", colorVal, sizeof(colorVal)) >= 0) {
      blue = atoi(colorVal);
    }
    
    _apiController->setClockColor(red, green, blue);
    _apiController->setClockMode(ClockMode::Clock);
  }
  
  // Timer control
  if (getUrlParamValue(params, values, paramCount, "TIMER", value, sizeof(value)) >= 0) {
    if (strcmp(value, "START") == 0) {
      int minTime = 5, maxTime = 7;
      char timeVal[16];
      
      if (getUrlParamValue(params, values, paramCount, "MIN_TIME", timeVal, sizeof(timeVal)) >= 0) {
        minTime = atoi(timeVal);
      }
      if (getUrlParamValue(params, values, paramCount, "MAX_TIME", timeVal, sizeof(timeVal)) >= 0) {
        maxTime = atoi(timeVal);
      }
      
      _apiController->startTimer(minTime, maxTime);
    } else if (strcmp(value, "STOP") == 0) {
      _apiController->stopTimer();
    }
  }
  
  // Test mode
  if (getUrlParamValue(params, values, paramCount, "TEST", value, sizeof(value)) >= 0) {
    _apiController->setClockMode(ClockMode::TestColors);
  }

  // Display mode control
  if (getUrlParamValue(params, values, paramCount, "MODE", value, sizeof(value)) >= 0) {
    if (strcmp(value, "CLOCK") == 0) {
      _apiController->setClockMode(ClockMode::Clock);
    }
  }
  
  // Redirect back to main page
  handleGetRoot(client);
}

void WebServer::handleApiStatus(WiFiClient& client) {
  char json[512];
  
  // Get current mode
  ClockMode mode = _apiController->getCurrentMode();
  const char* modeName = _apiController->getModeName(mode);
  
  // Get timer status
  bool timerRunning = _apiController->isTimerRunning();
  
  // Get time
  char timeStr[16];
  _apiController->getCurrentTime(timeStr, sizeof(timeStr));
  
  // Get LED state
  bool ledState = _apiController->getLedState();
  
  // Build JSON response
  snprintf(json, sizeof(json),
    "{"
    "\"mode\":\"%s\","
    "\"timer_running\":%s,"
    "\"time\":\"%s\","
    "\"led\":\"%s\","
    "\"time_set\":%s"
    "}",
    modeName,
    timerRunning ? "true" : "false",
    timeStr,
    ledState ? "on" : "off",
    _apiController->isTimeSet() ? "true" : "false"
  );
  
  sendJsonResponse(client, json);
}

void WebServer::handleApiDashboard(WiFiClient& client) {
  char systemJson[512];
  _apiController->getSystemInfo(systemJson, sizeof(systemJson));

  ClockMode mode = _apiController->getCurrentMode();
  const char* modeName = _apiController->getModeName(mode);
  bool timerRunning = _apiController->isTimerRunning();
  bool ledState = _apiController->getLedState();

  char timeStr[16];
  _apiController->getCurrentTime(timeStr, sizeof(timeStr));
  bool timeSet = _apiController->isTimeSet();

  uint8_t red = 255, green = 255, blue = 255;
  _apiController->getClockColor(red, green, blue);

  char json[1024];
  snprintf(json, sizeof(json),
    "{"
      "\"mode\":\"%s\","
      "\"mode_name\":\"%s\","
      "\"timer_running\":%s,"
      "\"led_on\":%s,"
      "\"current_time\":\"%s\","
      "\"time_set\":%s,"
      "\"clock_color\":{\"red\":%d,\"green\":%d,\"blue\":%d},"
      "\"system\":%s,"
      "\"request_count\":%lu"
    "}",
    modeName,
    modeName,
    timerRunning ? "true" : "false",
    ledState ? "true" : "false",
    timeStr,
    timeSet ? "true" : "false",
    red, green, blue,
    systemJson,
    _requestCount
  );

  sendJsonResponse(client, json);
}

void WebServer::handleApiSystem(WiFiClient& client) {
  char json[1024];
  _apiController->getSystemInfo(json, sizeof(json));
  sendJsonResponse(client, json);
}

void WebServer::handleApiTime(WiFiClient& client) {
  char timeStr[16];
  _apiController->getCurrentTime(timeStr, sizeof(timeStr));
  
  char json[128];
  snprintf(json, sizeof(json),
    "{\"time\":\"%s\",\"set\":%s}",
    timeStr,
    _apiController->isTimeSet() ? "true" : "false"
  );
  
  sendJsonResponse(client, json);
}

void WebServer::handle404(WiFiClient& client) {
  sendHttpHeader(client, "text/html", 404, "Not Found");
  client.println("<html><body><h1>404 Not Found</h1></body></html>");
}

// === HTTP Response Helpers ===

void WebServer::sendHttpHeader(WiFiClient& client, const char* contentType, 
                               int statusCode, const char* statusText) {
  client.printf("HTTP/1.1 %d %s\r\n", statusCode, statusText);
  client.printf("Content-Type: %s\r\n", contentType);
  client.println("Connection: close\r\n");
}

void WebServer::sendJsonResponse(WiFiClient& client, const char* json) {
  sendHttpHeader(client, "application/json");
  client.println(json);
}

// === Request Parsing ===

bool WebServer::parseHttpRequest(WiFiClient& client, char* method, char* path, 
                                 char* body, size_t bodySize) {
  if (!client.available()) return false;
  
  // Read first line: "GET /path HTTP/1.1"
  String firstLine = client.readStringUntil('\n');
  
  // Parse method and path
  int firstSpace = firstLine.indexOf(' ');
  int secondSpace = firstLine.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) return false;
  
  firstLine.substring(0, firstSpace).toCharArray(method, 16);
  firstLine.substring(firstSpace + 1, secondSpace).toCharArray(path, 128);
  
  // Read headers
  int contentLength = 0;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length:")) {
      contentLength = line.substring(15).toInt();
    }
    if (line == "\r" || line == "") break;
  }
  
  // Read body if present
  if (contentLength > 0 && client.available()) {
    int bytesToRead = min(contentLength, (int)bodySize - 1);
    client.readBytes(body, bytesToRead);
    body[bytesToRead] = '\0';
  }
  
  return true;
}

void WebServer::parseFormData(const char* body, char params[][32], char values[][64], 
                              int& count, int maxParams) {
  count = 0;
  if (body == nullptr || body[0] == '\0') return;
  
  char* bodyCopy = strdup(body);
  char* token = strtok(bodyCopy, "&");
  
  while (token != nullptr && count < maxParams) {
    char* equals = strchr(token, '=');
    if (equals != nullptr) {
      *equals = '\0';
      strncpy(params[count], token, 31);
      params[count][31] = '\0';
      
      strncpy(values[count], equals + 1, 63);
      values[count][63] = '\0';
      
      urlDecode(values[count]);
      count++;
    }
    token = strtok(nullptr, "&");
  }
  
  free(bodyCopy);
}

int WebServer::getUrlParamValue(const char params[][32], const char values[][64], 
                                int count, const char* key, char* outValue, size_t outSize) {
  for (int i = 0; i < count; i++) {
    if (strcmp(params[i], key) == 0) {
      strncpy(outValue, values[i], outSize - 1);
      outValue[outSize - 1] = '\0';
      return i;
    }
  }
  return -1;
}

void WebServer::urlDecode(char* str) {
  char* pstr = str;
  while (*str) {
    if (*str == '+') {
      *pstr++ = ' ';
    } else if (*str == '%' && str[1] && str[2]) {
      char hex[3] = {str[1], str[2], '\0'};
      *pstr++ = (char)strtol(hex, nullptr, 16);
      str += 2;
    } else {
      *pstr++ = *str;
    }
    str++;
  }
  *pstr = '\0';
}

void WebServer::serveFile(WiFiClient& client, const char* filepath, const char* contentType) {
  if (!_sdcard || !_sdcard->fileExists(filepath)) {
    DEBUGV("%s serveFile - File not found: %s\n", FILE_NAME_WEB, filepath);
    handle404(client);
    return;
  }
  
  File file = _sdcard->readFile(filepath);
  if (!file) {
    DEBUGV("%s serveFile - Failed to open file: %s\n", FILE_NAME_WEB, filepath);
    handle404(client);
    return;
  }
  
  size_t fileSize = file.size();
  DEBUGV("%s serveFile - Serving %s (%lu bytes)\n", FILE_NAME_WEB, filepath, (unsigned long)fileSize);
  
  // Send HTTP header with size for diagnostics
  client.printf("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: %s\r\n", contentType);
  client.printf("Content-Length: %lu\r\n", (unsigned long)fileSize);
  client.printf("X-File-Size: %lu\r\n", (unsigned long)fileSize);
  client.println("Connection: close\r\n");
  
  // Stream file content
  char buffer[256];
  while (file.available()) {
    int len = file.read((uint8_t*)buffer, sizeof(buffer));
    if (len > 0) {
      client.write((uint8_t*)buffer, len);
    }
  }
  
  file.close();
  DEBUGV("%s serveFile - Served: %s\n", FILE_NAME_WEB, filepath);
}

// === HTML Generation (DEPRECATED - HTML now served from SD card) ===

void WebServer::generateMainPage(char* buffer, size_t bufferSize) {
  // Get current state
  char timeStr[16];
  _apiController->getCurrentTime(timeStr, sizeof(timeStr));
  bool timeSet = _apiController->isTimeSet();
  
  ClockMode mode = _apiController->getCurrentMode();
  const char* modeName = _apiController->getModeName(mode);
  
  bool timerRunning = _apiController->isTimerRunning();
  bool ledState = _apiController->getLedState();
  
  // Get color
  uint8_t red, green, blue;
  _apiController->getClockColor(red, green, blue);
  
  // Get system info
  char sysJson[1024];
  _apiController->getSystemInfo(sysJson, sizeof(sysJson));
  
  // Parse system info for display (simple string search)
  float cpuTemp = 0.0f;
  char ipAddr[16] = "Unknown";
  bool wifiConnected = false;
  
  // Simple parsing - find values in JSON
  char* tempPos = strstr(sysJson, "\"temperature_c\":");
  if (tempPos) sscanf(tempPos + 16, "%f", &cpuTemp);
  
  char* ipPos = strstr(sysJson, "\"ip_address\":\"");
  if (ipPos) {
    sscanf(ipPos + 14, "%15[^\"]", ipAddr);
  }
  
  char* connPos = strstr(sysJson, "\"connected\":");
  if (connPos) {
    wifiConnected = (strstr(connPos, "true") != nullptr);
  }
  
  // Generate comprehensive HTML page
  snprintf(buffer, bufferSize,
    "<!DOCTYPE html>\n"
    "<html><head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>Speech Timer Dashboard</title>\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "<link rel=\"stylesheet\" href=\"/css/dashboard.css\">\n"
    "</head><body>\n"
    "<div class=\"container\">\n"
    "<h1>&#x1F3A4; Speech Timer Dashboard</h1>\n"
    "<div class=\"subtitle\">Real-time monitoring and control &bullet; <span id=\"refresh-status\">Updates paused</span></div>\n"
    
    "<!-- Status Dashboard -->\n"
    "<div class=\"dashboard\">\n"
    "  <div class=\"card\">\n"
    "    <h3>&#x23F0; Current Time</h3>\n"
    "    <div class=\"value\" id=\"time-value\">%s</div>\n"
    "    <div class=\"label\" id=\"time-label\">%s</div>\n"
    "  </div>\n"
    "  <div class=\"card\">\n"
    "    <h3>&#127918; Current Mode</h3>\n"
    "    <div class=\"value\" style=\"font-size:18px;\" id=\"mode-name\">%s</div>\n"
    "    <div class=\"status\"><div class=\"status-dot %s\" id=\"mode-dot\"></div><span id=\"mode-status\">%s</span></div>\n"
    "  </div>\n"
    "  <div class=\"card\">\n"
    "    <h3>&#x1F321; CPU Temperature</h3>\n"
    "    <div class=\"value\" id=\"temp-c\">%.1f&deg;C</div>\n"
    "    <div class=\"label\" id=\"temp-f\">%.1f&deg;F</div>\n"
    "  </div>\n"
    "  <div class=\"card\">\n"
    "    <h3>&#x1F3A8; Display Color</h3>\n"
    "    <div><span class=\"color-preview\" style=\"background:rgb(%d,%d,%d);\" id=\"color-preview\"></span></div>\n"
    "    <div class=\"label\" id=\"color-label\">RGB(%d, %d, %d)</div>\n"
    "  </div>\n"
    "</div>\n"
    
    "<!-- Speech Timer Control -->\n"
    "<div class=\"section\">\n"
    "  <h2>&#x23F1; Speech Timer Control</h2>\n"
    "  <form method=\"POST\" action=\"/\">\n"
    "    <div class=\"form-group\">\n"
    "      <label>Min Time:</label>\n"
    "      <input type=\"number\" name=\"MIN_TIME\" value=\"5\" min=\"1\" max=\"60\"> minutes\n"
    "    </div>\n"
    "    <div class=\"form-group\">\n"
    "      <label>Max Time:</label>\n"
    "      <input type=\"number\" name=\"MAX_TIME\" value=\"7\" min=\"1\" max=\"60\"> minutes\n"
    "    </div>\n"
    "    <button class=\"%s\" name=\"TIMER\" value=\"%s\" id=\"timer-btn\">%s %s Timer</button>\n"
    "  </form>\n"
    "</div>\n"
    
    "<!-- Clock Color Control -->\n"
    "<div class=\"section\">\n"
    "  <h2>&#x1F3A8; Clock Color Settings</h2>\n"
    "  <form method=\"POST\" action=\"/\">\n"
    "    <div class=\"form-group\">\n"
    "      <label>Red:</label>\n"
    "      <input type=\"number\" name=\"RED\" value=\"%d\" min=\"0\" max=\"255\">\n"
    "    </div>\n"
    "    <div class=\"form-group\">\n"
    "      <label>Green:</label>\n"
    "      <input type=\"number\" name=\"GREEN\" value=\"%d\" min=\"0\" max=\"255\">\n"
    "    </div>\n"
    "    <div class=\"form-group\">\n"
    "      <label>Blue:</label>\n"
    "      <input type=\"number\" name=\"BLUE\" value=\"%d\" min=\"0\" max=\"255\">\n"
    "    </div>\n"
    "    <button name=\"CLOCK\" value=\"SET\">Apply Clock Color</button>\n"
    "  </form>\n"
    "</div>\n"
    
    "<!-- System Controls -->\n"
    "<div class=\"section\">\n"
    "  <h2>&#9881;&#65039; System Controls</h2>\n"
    "  <form method=\"POST\" action=\"/\" style=\"display:inline;\">\n"
    "    <button class=\"%s\" name=\"LED\" value=\"%s\" id=\"led-btn\">%s LED %s</button>\n"
    "  </form>\n"
    "  <form method=\"POST\" action=\"/\" style=\"display:inline;\">\n"
    "    <button name=\"TEST\" value=\"START\">&#128300; Start Test Mode</button>\n"
    "  </form>\n"
    "</div>\n"
    
    "<!-- System Information -->\n"
    "<div class=\"section\">\n"
    "  <h2>&#128200; System Information</h2>\n"
    "  <div class=\"info-grid\">\n"
    "    <div class=\"info-item\"><span class=\"info-label\">WiFi Status:</span><span class=\"info-value\" id=\"wifi-status\">%s</span></div>\n"
    "    <div class=\"info-item\"><span class=\"info-label\">IP Address:</span><span class=\"info-value\" id=\"ip-addr\">%s</span></div>\n"
    "    <div class=\"info-item\"><span class=\"info-label\">Requests:</span><span class=\"info-value\" id=\"request-count\">%lu</span></div>\n"
    "    <div class=\"info-item\"><span class=\"info-label\">LED Status:</span><span class=\"info-value\" id=\"led-status\">%s</span></div>\n"
    "  </div>\n"
    "</div>\n"
    
    "<!-- API Links -->\n"
    "<div class=\"api-links\">\n"
    "  <strong>JSON API Endpoints:</strong><br>\n"
    "  <a href=\"/api/status\" target=\"_blank\">Status</a> |\n"
    "  <a href=\"/api/system\" target=\"_blank\">System</a> |\n"
    "  <a href=\"/api/time\" target=\"_blank\">Time</a>\n"
    "</div>\n"
    
    "</div>\n"
    "<script src=\"/js/dashboard.js\"></script>\n"
    "</body></html>",
    // Status dashboard values
    timeStr,
    timeSet ? "Time synchronized" : "Time not set",
    modeName,
    timerRunning ? "status-online" : "status-offline",
    timerRunning ? "Timer Running" : "Ready",
    cpuTemp,
    cpuTemp * 1.8f + 32.0f,
    red, green, blue,
    red, green, blue,
    // Timer control
    timerRunning ? "danger" : "success",
    timerRunning ? "STOP" : "START",
    timerRunning ? "&#x23F9;" : "&#x25B6;",
    timerRunning ? "Stop" : "Start",
    // Clock color (current values)
    red, green, blue,
    // System controls
    ledState ? "danger" : "success",
    ledState ? "OFF" : "ON",
    ledState ? "&#x1F4A1;" : "&#x1F4A1;",
    ledState ? "Off" : "On",
    // System info
    wifiConnected ? "&#x2705; Connected" : "&#x274C; Disconnected",
    ipAddr,
    _requestCount,
    ledState ? "On" : "Off"
  );
}
