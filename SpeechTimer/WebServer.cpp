#include "WebServer.h"
#include "Defines.h"
#include "Clk_SdCard.h"
#include <cstring>
#include <cstdlib>

#define FILE_NAME_WEB "[WebServer.cpp]"
#define REQUEST_BUFFER_SIZE 2048
#define RESPONSE_BUFFER_SIZE 2048

// Latency-tolerant HTTP timing (milliseconds)
static constexpr unsigned long HTTP_FIRST_LINE_TIMEOUT_MS = 5000;
static constexpr unsigned long HTTP_HEADER_LINE_TIMEOUT_MS = 2500;
static constexpr unsigned long HTTP_BODY_TIMEOUT_MS = 5000;
static constexpr unsigned long HTTP_WRITE_STALL_TIMEOUT_MS = 15000;
static constexpr unsigned long HTTP_CLOSE_GRACE_MS = 8000;

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
    DBG_VERBOSE("%s begin - Server started on port %d\n", FILE_NAME_WEB, _port);
  } else {
    DBG_VERBOSE("%s begin - Failed to initialize (null pointers)\n", FILE_NAME_WEB);
  }
}

void WebServer::stop() {
  _server.stop();
  _initialized = false;
  DBG_VERBOSE("%s stop - Server stopped\n", FILE_NAME_WEB);
}

bool WebServer::isRunning() {
  return _initialized;
}

// === Main Request Handler ===

void WebServer::handleClient() {
  if (!_initialized) return;

  // Drain multiple pending clients per scheduler tick.
  // Use available() so we only process clients that already have request bytes.
  const int maxClientsPerCall = 4;
  for (int clientIndex = 0; clientIndex < maxClientsPerCall; clientIndex++) {
    WiFiClient client = _server.available();
    if (!client) {
      break;
    }

    DBG_VERBOSE("%s handleClient - New client connected\n", FILE_NAME_WEB);

    // Reduce packet coalescing delays on slower/higher-latency links.
    client.setNoDelay(true);

    _requestCount++;
    DBG_VERBOSE("%s handleClient - Request count: %d\n", FILE_NAME_WEB, _requestCount);
    DBG_VERBOSE("%s handleClient - Client has data, processing request\n", FILE_NAME_WEB);
    handleRequest(client);

    // Give TCP stack time to finish transmitting on slower links.
    unsigned long closeDeadline = millis() + HTTP_CLOSE_GRACE_MS;
    while (client.connected() && millis() < closeDeadline) {
      delay(2);
    }

    client.stop();
    DBG_VERBOSE("%s handleClient - Client disconnected\n", FILE_NAME_WEB);
  }
}

void WebServer::handleRequest(WiFiClient& client) {
  char method[16] = {0};
  char path[256] = {0};
  char body[512] = {0};
  
  if (!parseHttpRequest(client, method, path, sizeof(path), body, sizeof(body))) {
    DBG_VERBOSE("%s handleRequest - Failed to parse request\n", FILE_NAME_WEB);
    return;
  }
  
  DBG_VERBOSE("%s handleRequest - %s %s\n", FILE_NAME_WEB, method, path);

  // Strip query string for route matching (e.g., /js/dashboard.js?v=123)
  char* queryStart = strchr(path, '?');
  if (queryStart != nullptr) {
    *queryStart = '\0';
  }
  
  // Route the request
  if (strcmp(method, "GET") == 0) {
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
      handleGetRoot(client);
    } else if (strcmp(path, "/favicon.ico") == 0) {
      client.print("HTTP/1.1 204 No Content\r\n");
      client.print("Content-Length: 0\r\n");
      client.print("Connection: close\r\n");
      client.print("\r\n");
    } else if (strcmp(path, "/api/dashboard") == 0) {
      handleApiDashboard(client);
    } else if (strcmp(path, "/api/status") == 0) {
      handleApiStatus(client);
    } else if (strcmp(path, "/api/system") == 0) {
      handleApiSystem(client);
    } else if (strcmp(path, "/api/time") == 0) {
      handleApiTime(client);
    } else if (tryServeStaticGet(client, path)) {
      // Served as static content from SD card.
    } else {
      DBG_VERBOSE("%s handleRequest - Unknown GET path: %s\n", FILE_NAME_WEB, path);
      handle404(client);
    }
  } else if (strcmp(method, "POST") == 0) {
    if (strcmp(path, "/") == 0) {
      handlePostRoot(client, body);
    } else {
      DBG_VERBOSE("%s handleRequest - Unknown POST path: %s\n", FILE_NAME_WEB, path);
      handle404(client);
    }
  } else {
    DBG_VERBOSE("%s handleRequest - Unknown method: %s\n", FILE_NAME_WEB, method);
    handle404(client);
  }
}

// === Route Handlers ===

void WebServer::handleGetRoot(WiFiClient& client) {
  // Serve static index.html from SD card
  DBG_VERBOSE("%s handleGetRoot - Attempting to serve index.html from SD card\n", FILE_NAME_WEB);
  if (!_sdcard) {
    DBG_VERBOSE("%s handleGetRoot - ERROR: SD Card not initialized!\n", FILE_NAME_WEB);
    sendHttpHeader(client, "text/html", 503, "Service Unavailable");
    client.println("<html><body>");
    client.println("<h1>503 Service Unavailable</h1>");
    client.println("<p>SD Card not initialized. Please check SD card connection.</p>");
    client.println("</body></html>");
    return;
  }
  if (!tryServeStaticGet(client, "/")) {
    DBG_VERBOSE("%s handleGetRoot - index.html not found on SD card\n", FILE_NAME_WEB);
    sendHttpHeader(client, "text/html", 404, "Not Found");
    client.println("<html><body>");
    client.println("<h1>404 Not Found</h1>");
    client.println("<p>index.html not found on SD card at: wwwroot/index.html</p>");
    client.println("<p>Available API endpoints:</p>");
    client.println("<ul>");
    client.println("<li>/api/dashboard - Dashboard data</li>");
    client.println("<li>/api/status - Status information</li>");
    client.println("<li>/api/system - System information</li>");
    client.println("<li>/api/time - Current time</li>");
    client.println("</ul>");
    client.println("</body></html>");
  }
}

void WebServer::handlePostRoot(WiFiClient& client, const char* body) {
  // Parse form data
  char params[10][32];
  char values[10][64];
  int paramCount = 0;
  
  DBG_VERBOSE("%s handlePostRoot - Received body: '%s'\n", FILE_NAME_WEB, body);
  DBG_VERBOSE("%s handlePostRoot - Body length: %d\n", FILE_NAME_WEB, strlen(body));
  
  parseFormData(body, params, values, paramCount, 10);
  
  DBG_VERBOSE("%s handlePostRoot - Parsed %d parameters from body\n", FILE_NAME_WEB, paramCount);
  for (int i = 0; i < paramCount; i++) {
    DBG_VERBOSE("%s handlePostRoot - Param[%d]: '%s' = '%s'\n", FILE_NAME_WEB, i, params[i], values[i]);
  }
  
  // Process form data
  char value[64] = {0};
  
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
    DBG_VERBOSE("%s handlePostRoot - TIMER parameter found: %s\n", FILE_NAME_WEB, value);
    if (strcmp(value, "START") == 0) {
      int minTime = 5, maxTime = 7;
      char timeVal[16] = {0};
      
      int minIdx = getUrlParamValue(params, values, paramCount, "MIN_TIME", timeVal, sizeof(timeVal));
      if (minIdx >= 0) {
        minTime = atoi(timeVal);
        DBG_VERBOSE("%s handlePostRoot - MIN_TIME found at index %d: '%s' = %d\n", FILE_NAME_WEB, minIdx, timeVal, minTime);
      } else {
        DBG_VERBOSE("%s handlePostRoot - MIN_TIME not found, using default: %d\n", FILE_NAME_WEB, minTime);
      }
      
      int maxIdx = getUrlParamValue(params, values, paramCount, "MAX_TIME", timeVal, sizeof(timeVal));
      if (maxIdx >= 0) {
        maxTime = atoi(timeVal);
        DBG_VERBOSE("%s handlePostRoot - MAX_TIME found at index %d: '%s' = %d\n", FILE_NAME_WEB, maxIdx, timeVal, maxTime);
      } else {
        DBG_VERBOSE("%s handlePostRoot - MAX_TIME not found, using default: %d\n", FILE_NAME_WEB, maxTime);
      }
      
      DBG_VERBOSE("%s handlePostRoot - Starting timer: minTime=%d, maxTime=%d\n", FILE_NAME_WEB, minTime, maxTime);
      _apiController->startTimer(minTime, maxTime);
    } else if (strcmp(value, "STOP") == 0) {
      DBG_VERBOSE("%s handlePostRoot - Stopping timer\n", FILE_NAME_WEB);
      _apiController->stopTimer();
    }
  } else {
    DBG_VERBOSE("%s handlePostRoot - TIMER parameter NOT found\n", FILE_NAME_WEB);
  }
  
  // Test mode
  if (getUrlParamValue(params, values, paramCount, "TEST", value, sizeof(value)) >= 0) {
    DBG_VERBOSE("%s handlePostRoot - TEST parameter found: %s\n", FILE_NAME_WEB, value);
    if (strcmp(value, "START") == 0) {
      DBG_VERBOSE("%s handlePostRoot - Starting Test Mode (TestColors)\n", FILE_NAME_WEB);
      _apiController->setClockMode(ClockMode::TestColors);
    } else {
      DBG_VERBOSE("%s handlePostRoot - WARNING: Unknown TEST value: %s\n", FILE_NAME_WEB, value);
    }
  }

  // Display mode control
  if (getUrlParamValue(params, values, paramCount, "MODE", value, sizeof(value)) >= 0) {
    if (strcmp(value, "CLOCK") == 0) {
      _apiController->setClockMode(ClockMode::Clock);
    }
  }
  
  // Send simple JSON response - don't try to serve entire HTML file
  DBG_VERBOSE("%s handlePostRoot - Sending response\n", FILE_NAME_WEB);
  sendHttpHeader(client, "application/json");
  client.println("{\"status\":\"ok\"}");
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
  
  // Build JSON response
  snprintf(json, sizeof(json),
    "{"
    "\"mode\":\"%s\","
    "\"timer_running\":%s,"
    "\"time\":\"%s\","
    "\"time_set\":%s"
    "}",
    modeName,
    timerRunning ? "true" : "false",
    timeStr,
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
      "\"current_time\":\"%s\","
      "\"time_set\":%s,"
      "\"clock_color\":{\"red\":%d,\"green\":%d,\"blue\":%d},"
      "\"system\":%s,"
      "\"request_count\":%lu"
    "}",
    modeName,
    modeName,
    timerRunning ? "true" : "false",
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
  client.println("Cache-Control: no-store, no-cache, must-revalidate\r");
  client.println("Connection: close\r\n");
}

void WebServer::sendJsonResponse(WiFiClient& client, const char* json) {
  const size_t jsonLen = strlen(json);
  client.printf("HTTP/1.1 200 OK\r\n");
  client.println("Content-Type: application/json\r");
  client.printf("Content-Length: %lu\r\n", (unsigned long)jsonLen);
  client.println("Cache-Control: no-store, no-cache, must-revalidate\r");
  client.println("Connection: close\r\n");
  client.println(json);
}

// === Request Parsing ===

bool WebServer::parseHttpRequest(WiFiClient& client, char* method, char* path, size_t pathSize,
                                 char* body, size_t bodySize) {
  if (pathSize == 0) {
    return false;
  }

  // Initialize body buffer
  body[0] = '\0';
  
  // Read first line: "GET /path HTTP/1.1"
  String firstLine = "";
  bool firstLineDone = false;
  unsigned long firstLineLastProgress = millis();
  while ((millis() - firstLineLastProgress) < HTTP_FIRST_LINE_TIMEOUT_MS && !firstLineDone) {
    while (client.available()) {
      char c = (char)client.read();
      firstLineLastProgress = millis();
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        firstLineDone = true;
        break;
      }
      if (firstLine.length() < 255) {
        firstLine += c;
      }
    }

    if (firstLineDone) {
      break;
    }

    if (!client.connected() && !client.available()) {
      break;
    }

    delay(1);
  }
  
  if (firstLine.length() == 0) {
    DBG_VERBOSE("%s parseHttpRequest - Empty first line (likely idle/preconnect)\n", FILE_NAME_WEB);
    return false;
  }
  
  // Parse method and path
  int firstSpace = firstLine.indexOf(' ');
  int secondSpace = firstLine.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) {
    DBG_VERBOSE("%s parseHttpRequest - Malformed request: %s\n", FILE_NAME_WEB, firstLine.c_str());
    return false;
  }
  
  firstLine.substring(0, firstSpace).toCharArray(method, 16);
  firstLine.substring(firstSpace + 1, secondSpace).toCharArray(path, pathSize);
  
  // Read headers
  int contentLength = 0;
  while (true) {
    String line = "";
    bool lineDone = false;
    unsigned long lineLastProgress = millis();

    while ((millis() - lineLastProgress) < HTTP_HEADER_LINE_TIMEOUT_MS && !lineDone) {
      while (client.available()) {
        char c = (char)client.read();
        lineLastProgress = millis();
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          lineDone = true;
          break;
        }
        if (line.length() < 255) {
          line += c;
        }
      }

      if (lineDone) {
        break;
      }

      if (!client.connected() && !client.available()) {
        break;
      }

      delay(1);
    }

    // Timeout/closed before next header line: treat as end-of-headers
    if (!lineDone && line.length() == 0) {
      break;
    }

    if (line.startsWith("Content-Length:")) {
      contentLength = line.substring(15).toInt();
      DBG_VERBOSE("%s parseHttpRequest - Found Content-Length: %d\n", FILE_NAME_WEB, contentLength);
    }

    // Empty line marks end of headers
    if (line.length() == 0) {
      break;
    }
  }
  
  // Read body if present
  if (contentLength > 0) {
    int bytesToRead = min(contentLength, (int)bodySize - 1);
    DBG_VERBOSE("%s parseHttpRequest - Expecting %d bytes, buffer size %d\n", FILE_NAME_WEB, contentLength, bodySize);

    int bytesRead = 0;
    unsigned long bodyLastProgress = millis();
    while (bytesRead < bytesToRead && (millis() - bodyLastProgress) < HTTP_BODY_TIMEOUT_MS) {
      while (client.available() && bytesRead < bytesToRead) {
        body[bytesRead++] = (char)client.read();
        bodyLastProgress = millis();
      }

      if (bytesRead >= bytesToRead) {
        break;
      }

      if (!client.connected() && !client.available()) {
        break;
      }

      delay(1);
    }

    body[bytesRead] = '\0';
    DBG_VERBOSE("%s parseHttpRequest - Read %d bytes: '%s'\n", FILE_NAME_WEB, bytesRead, body);
  } else {
    DBG_VERBOSE("%s parseHttpRequest - No Content-Length or length=0\n", FILE_NAME_WEB);
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

const char* WebServer::getContentTypeForPath(const char* path) {
  const char* ext = strrchr(path, '.');
  if (!ext) {
    return "application/octet-stream";
  }

  if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
  if (strcmp(ext, ".css") == 0) return "text/css";
  if (strcmp(ext, ".js") == 0 || strcmp(ext, ".mjs") == 0) return "text/javascript";
  if (strcmp(ext, ".json") == 0) return "application/json";
  if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
  if (strcmp(ext, ".png") == 0) return "image/png";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
  if (strcmp(ext, ".gif") == 0) return "image/gif";
  if (strcmp(ext, ".webp") == 0) return "image/webp";
  if (strcmp(ext, ".ico") == 0) return "image/x-icon";
  if (strcmp(ext, ".wasm") == 0) return "application/wasm";
  if (strcmp(ext, ".woff") == 0) return "font/woff";
  if (strcmp(ext, ".woff2") == 0) return "font/woff2";
  if (strcmp(ext, ".ttf") == 0) return "font/ttf";
  if (strcmp(ext, ".otf") == 0) return "font/otf";
  if (strcmp(ext, ".xml") == 0) return "application/xml";
  if (strcmp(ext, ".txt") == 0) return "text/plain";
  if (strcmp(ext, ".map") == 0) return "application/json";
  return "application/octet-stream";
}

bool WebServer::tryServeStaticGet(WiFiClient& client, const char* path) {
  if (path == nullptr || path[0] != '/') {
    return false;
  }

  char decodedPath[256] = {0};
  strncpy(decodedPath, path, sizeof(decodedPath) - 1);
  decodedPath[sizeof(decodedPath) - 1] = '\0';
  urlDecode(decodedPath);

  // Prevent path traversal outside wwwroot and reject Windows separators.
  if (strstr(decodedPath, "..") != nullptr || strchr(decodedPath, '\\') != nullptr) {
    DBG_VERBOSE("%s tryServeStaticGet - Rejected traversal path: %s\n", FILE_NAME_WEB, decodedPath);
    return false;
  }

  // API routes are handled explicitly elsewhere.
  if (strncmp(decodedPath, "/api/", 5) == 0) {
    return false;
  }

  if (!_sdcard) {
    return false;
  }

  char staticPath[256] = {0};
  if (strcmp(decodedPath, "/") == 0) {
    strncpy(staticPath, "wwwroot/index.html", sizeof(staticPath) - 1);
  } else {
    snprintf(staticPath, sizeof(staticPath), "wwwroot%s", decodedPath);
  }

  // If URL ends with '/', serve index.html in that folder.
  size_t pathLen = strlen(staticPath);
  if (pathLen > 0 && staticPath[pathLen - 1] == '/') {
    strncat(staticPath, "index.html", sizeof(staticPath) - strlen(staticPath) - 1);
  }

  // Mirror serveFile's relative/absolute fallback behavior.
  const char* resolvedPath = staticPath;
  char altPath[256] = {0};
  if (!_sdcard->fileExists(resolvedPath)) {
    if (staticPath[0] != '/' && strlen(staticPath) < sizeof(altPath) - 1) {
      altPath[0] = '/';
      strncpy(altPath + 1, staticPath, sizeof(altPath) - 2);
      altPath[sizeof(altPath) - 1] = '\0';
      if (_sdcard->fileExists(altPath)) {
        resolvedPath = altPath;
      }
    }
  }

  if (!_sdcard->fileExists(resolvedPath)) {
    DBG_VERBOSE("%s tryServeStaticGet - Static miss: req=%s decoded=%s mapped=%s\n", FILE_NAME_WEB, path, decodedPath, staticPath);
    return false;
  }

  const char* contentType = getContentTypeForPath(resolvedPath);
  DBG_VERBOSE("%s tryServeStaticGet - Static hit: req=%s decoded=%s resolved=%s type=%s\n", FILE_NAME_WEB, path, decodedPath, resolvedPath, contentType);
  serveFile(client, resolvedPath, contentType);
  return true;
}

void WebServer::serveFile(WiFiClient& client, const char* filepath, const char* contentType) {
  if (!_sdcard) {
    DBG_VERBOSE("%s serveFile - SD Card not initialized for: %s\n", FILE_NAME_WEB, filepath);
    handle404(client);
    return;
  }

  if (!_sdcard->beginIo(pdMS_TO_TICKS(MUTEX_TIMEOUT_MS))) {
    DBG_VERBOSE("%s serveFile - SD Card busy for: %s\n", FILE_NAME_WEB, filepath);
    sendHttpHeader(client, "text/html", 503, "Service Unavailable");
    client.println("<html><body><h1>503 Service Unavailable</h1><p>SD Card busy. Please retry.</p></body></html>");
    return;
  }

  const char* resolvedPath = filepath;
  char altPath[256] = {0};
  if (!_sdcard->fileExists(resolvedPath)) {
    if (filepath[0] != '/' && strlen(filepath) < sizeof(altPath) - 1) {
      altPath[0] = '/';
      strncpy(altPath + 1, filepath, sizeof(altPath) - 2);
      altPath[sizeof(altPath) - 1] = '\0';
      if (_sdcard->fileExists(altPath)) {
        resolvedPath = altPath;
      }
    }
  }

  if (!_sdcard->fileExists(resolvedPath)) {
    DBG_VERBOSE("%s serveFile - File not found: %s\n", FILE_NAME_WEB, filepath);
    _sdcard->endIo();
    handle404(client);
    return;
  }
  
  File file = _sdcard->readFile(resolvedPath);
  if (!file) {
    DBG_VERBOSE("%s serveFile - Failed to open file: %s\n", FILE_NAME_WEB, resolvedPath);
    _sdcard->endIo();
    handle404(client);
    return;
  }
  
  size_t fileSize = file.size();
  DBG_VERBOSE("%s serveFile - Serving %s (%lu bytes) as %s\n", FILE_NAME_WEB, resolvedPath, (unsigned long)fileSize, contentType);
  
  // Send HTTP header with correct formatting
  client.printf("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: %s\r\n", contentType);
  client.printf("Content-Length: %lu\r\n", (unsigned long)fileSize);

  if (strcmp(contentType, "text/css") == 0 || strcmp(contentType, "text/javascript") == 0) {
    client.printf("Cache-Control: public, max-age=3600\r\n");
  } else {
    client.printf("Cache-Control: no-store, no-cache, must-revalidate\r\n");
  }

  client.printf("Connection: close\r\n");
  client.printf("\r\n");  // Blank line between headers and body
  
  // Stream file content
  char buffer[128];
  size_t bytesSent = 0;
  bool transferAborted = false;
  while (file.available()) {
    int len = file.read((uint8_t*)buffer, sizeof(buffer));
    if (len > 0) {
      int offset = 0;
      unsigned long writeLastProgress = millis();
      while (offset < len) {
        size_t written = client.write((uint8_t*)buffer + offset, len - offset);
        if (written == 0) {
          if (!client.connected() || (millis() - writeLastProgress) >= HTTP_WRITE_STALL_TIMEOUT_MS) {
            DBG_VERBOSE("%s serveFile - Client write stalled for: %s\n", FILE_NAME_WEB, filepath);
            transferAborted = true;
            break;
          }
          delay(1);
          continue;
        }
        offset += (int)written;
        bytesSent += written;
        writeLastProgress = millis();
      }

      if (offset < len) {
        transferAborted = true;
        break;
      }
    } else {
      break;
    }
    delay(0);
  }

  client.flush();
  if (bytesSent != fileSize || transferAborted) {
    DBG_VERBOSE("%s serveFile - WARNING: bytesSent(%lu) != fileSize(%lu) for %s\n", FILE_NAME_WEB, (unsigned long)bytesSent, (unsigned long)fileSize, filepath);
  }

  file.close();
  _sdcard->endIo();
  DBG_VERBOSE("%s serveFile - Served: %s\n", FILE_NAME_WEB, filepath);
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
