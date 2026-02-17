#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include "ApiController.h"
#include "DbgPrint.h"

// Forward declarations
class Clk_SdCard;

/**
 * Web Server - Transport Layer
 * 
 * This class handles HTTP requests and translates them into API controller calls.
 * It serves as the transport layer between the web UI and the business logic.
 * 
 * Supports:
 * - Static file serving (HTML/CSS/JS from SD card)
 * - Form POST handling
 * - JSON API endpoints for programmatic access
 * 
 * API Endpoints:
 * - GET  / - Main web page
 * - POST / - Form submissions
 * - GET  /api/status - JSON status response
 * - GET  /api/system - JSON system info
 * - GET  /api/time - JSON time info
 */
class WebServer {
public:
  // Constructor
  WebServer(uint16_t port = 80);
  
  // Initialize with API controller reference
  void begin(ApiController* apiController, Clk_SdCard* sdcard);
  
  // Main server loop - call this regularly from main task
  void handleClient();
  
  // Server control
  void stop();
  bool isRunning();

private:
  WiFiServer _server;
  ApiController* _apiController;
  Clk_SdCard* _sdcard;
  bool _initialized;
  uint16_t _port;
  unsigned long _requestCount;
  
  // HTTP Request handling
  void handleRequest(WiFiClient& client);
  void handleGetRoot(WiFiClient& client);
  void handlePostRoot(WiFiClient& client, const char* body);
  void handleApiStatus(WiFiClient& client);
  void handleApiDashboard(WiFiClient& client);
  void handleApiSystem(WiFiClient& client);
  void handleApiTime(WiFiClient& client);
  void handle404(WiFiClient& client);
  void serveFile(WiFiClient& client, const char* filepath, const char* contentType);
  
  // HTTP Response helpers
  void sendHttpHeader(WiFiClient& client, const char* contentType = "text/html", 
                     int statusCode = 200, const char* statusText = "OK");
  void sendJsonResponse(WiFiClient& client, const char* json);
  
  // Request parsing
  bool parseHttpRequest(WiFiClient& client, char* method, char* path, char* body, size_t bodySize);
  void parseFormData(const char* body, char params[][32], char values[][64], int& count, int maxParams);
  
  // HTML generation (DEPRECATED - HTML now served from SD card)
  void generateMainPage(char* buffer, size_t bufferSize);
  
  // Utility
  void urlDecode(char* str);
  int getUrlParamValue(const char params[][32], const char values[][64], int count, 
                       const char* key, char* outValue, size_t outSize);
};

#endif // WEB_SERVER_H
