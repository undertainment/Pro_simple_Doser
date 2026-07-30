#include "apex.h"
#include "logger.h"
#include <WiFi.h>

ApexConfig Apex::_config;
ApexProbe Apex::_probes[APEX_MAX_PROBES];
uint8_t Apex::_probeCount = 0;
unsigned long Apex::_lastUpdate = 0;
unsigned long Apex::_lastPoll = 0;
bool Apex::_connected = false;

void Apex::init() {
  _config.port = 80;
  _config.enabled = false;
  _config.ip[0] = '\0';
  strcpy(_config.username, "admin");
  _config.password[0] = '\0';
  _probeCount = 0;
  _lastUpdate = 0;
  _lastPoll = -APEX_POLL_INTERVAL_MS;
  Logger::info(F("Apex Classic client initialized"));
}

void Apex::loop() {
  if (!_config.enabled || _config.ip[0] == '\0') return;
  unsigned long now = millis();
  if (now - _lastPoll < APEX_POLL_INTERVAL_MS) return;
  _lastPoll = now;
  _poll();
}

void Apex::setConfig(const ApexConfig& cfg) {
  _config = cfg;
}

const ApexConfig& Apex::getConfig() {
  return _config;
}

bool Apex::isConnected() {
  return _connected;
}

unsigned long Apex::lastUpdate() {
  return _lastUpdate;
}

const ApexProbe* Apex::getProbes() {
  return _probes;
}

uint8_t Apex::probeCount() {
  return _probeCount;
}

// ── base64 ───────────────────────────────────────────

static const char _b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String _base64Encode(const uint8_t* data, size_t len) {
  String out;
  for (size_t i = 0; i < len; i += 3) {
    int b = (data[i] << 16) | (data[i + 1 < len ? i + 1 : i] << 8) | (data[i + 2 < len ? i + 2 : i]);
    out += _b64[(b >> 18) & 0x3F];
    out += _b64[(b >> 12) & 0x3F];
    out += (i + 1 < len) ? _b64[(b >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? _b64[b & 0x3F] : '=';
  }
  return out;
}

static int _readResponse(WiFiClient& c, String& out, unsigned long timeoutMs) {
  unsigned long deadline = millis() + timeoutMs;
  out = "";
  while (millis() < deadline) {
    while (c.available()) {
      out += (char)c.read();
    }
    if (!c.connected() && !c.available()) break;
    delay(2);
  }
  return out.length();
}

// ── poll ─────────────────────────────────────────────

void Apex::_poll() {
  WiFiClient client;
  String path = "/cgi-bin/status.xml";
  String host = String(_config.ip);
  String auth = String(_config.username) + ":" + String(_config.password);
  String authB64 = _base64Encode((const uint8_t*)auth.c_str(), auth.length());

  if (!client.connect(_config.ip, _config.port, APEX_TIMEOUT_MS)) {
    _connected = false;
    Logger::warn(F("Apex: connection failed"));
    return;
  }

  String req = "GET " + path + " HTTP/1.1\r\n"
               "Host: " + host + "\r\n"
               "Authorization: Basic " + authB64 + "\r\n"
               "Connection: close\r\n\r\n";
  client.print(req);

  String resp;
  _readResponse(client, resp, APEX_TIMEOUT_MS);
  client.stop();

  if (resp.length() == 0) {
    _connected = false;
    Logger::warn(F("Apex: empty response"));
    return;
  }

  int spaceIdx = resp.indexOf(' ');
  if (spaceIdx < 0) { _connected = false; return; }
  String statusCode = resp.substring(spaceIdx + 1, spaceIdx + 4);

  if (statusCode != "200") {
    Logger::warn(String(F("Apex: HTTP ")) + statusCode);
    _connected = false;
    return;
  }

  // --- extract body ---
  int bodyStart = resp.indexOf("\r\n\r\n");
  if (bodyStart < 0) bodyStart = resp.indexOf("\n\n");
  if (bodyStart < 0) { _connected = false; return; }
  bodyStart += (resp[bodyStart + 1] == '\n' ? 2 : 4);
  String body = resp.substring(bodyStart);
  body.trim();

  // --- parse XML probes ---
  _probeCount = 0;
  int pos = 0;
  while (_probeCount < APEX_MAX_PROBES) {
    int ps = body.indexOf("<probe>", pos);
    if (ps < 0) break;
    int pe = body.indexOf("</probe>", ps);
    if (pe < 0) break;
    String block = body.substring(ps + 7, pe);
    pos = pe + 8;

    int ni = block.indexOf("<name>");
    if (ni < 0) continue;
    ni += 6;
    int ne = block.indexOf("</name>", ni);
    if (ne < 0) continue;
    String pname = block.substring(ni, ne);
    pname.trim();
    if (pname.length() == 0) continue;

    int vi = block.indexOf("<value>");
    if (vi < 0) continue;
    vi += 7;
    int ve = block.indexOf("</value>", vi);
    if (ve < 0) continue;
    String pval = block.substring(vi, ve);
    pval.trim();

    ApexProbe& probe = _probes[_probeCount];
    strncpy(probe.name, pname.c_str(), sizeof(probe.name) - 1);
    probe.name[sizeof(probe.name) - 1] = '\0';
    probe.value = pval.toFloat();

    if (pname == "pH") strcpy(probe.label, "pH");
    else if (pname == "Tmp" || pname == "Temp" || pname == "Temperature") strcpy(probe.label, "Temp");
    else if (pname == "ORP") strcpy(probe.label, "ORP");
    else if (pname == "Salinity" || pname == "Sal") strcpy(probe.label, "Salinity");
    else if (pname == "Amp_3" || pname == "Amps") strcpy(probe.label, "Amps");
    else if (pname == "ALK" || pname == "Alkalinity") strcpy(probe.label, "Alk");
    else if (pname == "Ca" || pname == "Calcium") strcpy(probe.label, "Calcium");
    else if (pname == "Mg" || pname == "Magnesium") strcpy(probe.label, "Magnesium");
    else strncpy(probe.label, pname.c_str(), sizeof(probe.label) - 1);

    _probeCount++;
  }

  _connected = (_probeCount > 0);
  _lastUpdate = millis();

  if (_probeCount > 0) {
    Logger::info(String(F("Apex Classic: ")) + _probeCount + F(" probes OK"));
  } else {
    Logger::warn(F("Apex Classic: no <probe> elements found"));
  }
}
