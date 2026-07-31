#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

class HttpServer {
public:
  static void init();
  static void loop();
  static void checkWiFi();

private:
  static WebServer _server;
  static bool _apMode;

  static void _handleRoot();
  static void _handleAPI();
  static void _handleDose();
  static void _handlePumpConfig();
  static void _handleScheduleAdd();
  static void _handleScheduleRemove();
  static void _handleResetTotals();
  static void _handleRefill();
  static void _handleApexConfig();
  static void _handleNTP();
  static void _handleTimeZone();
  static void _handleConfigExport();
  static void _handleConfigImport();
  static void _handleNotFound();

  static String _buildDashboardHTML();
};

#endif
