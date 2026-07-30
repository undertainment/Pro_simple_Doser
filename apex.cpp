#include "apex.h"
#include "logger.h"
#include <WiFi.h>

Apex::UnitState Apex::_units[APEX_UNIT_COUNT];

void Apex::init() {
  for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
    _units[u].config.port = 80;
    _units[u].config.enabled = false;
    _units[u].config.ip[0] = '\0';
    strcpy(_units[u].config.username, "admin");
    _units[u].config.password[0] = '\0';
    _units[u].config.probeMask = 0xFF;
    _units[u].probeCount = 0;
    _units[u].lastUpdate = 0;
    _units[u].lastPoll = -APEX_POLL_INTERVAL_MS;
    _units[u].connected = false;
  }
  Logger::info(F("Apex Classic: 2-unit client initialized"));
}

void Apex::loop() {
  unsigned long now = millis();
  for (uint8_t u = 0; u < APEX_UNIT_COUNT; u++) {
    UnitState& unit = _units[u];
    if (!unit.config.enabled || unit.config.ip[0] == '\0') continue;
    if (now - unit.lastPoll < APEX_POLL_INTERVAL_MS) continue;
    unit.lastPoll = now;
    _poll(u);
  }
}

void Apex::setConfig(uint8_t unit, const ApexConfig& cfg) {
  if (unit < APEX_UNIT_COUNT) _units[unit].config = cfg;
}

const ApexConfig& Apex::getConfig(uint8_t unit) {
  return _units[unit < APEX_UNIT_COUNT ? unit : 0].config;
}

bool Apex::isConnected(uint8_t unit) {
  return unit < APEX_UNIT_COUNT ? _units[unit].connected : false;
}

unsigned long Apex::lastUpdate(uint8_t unit) {
  return unit < APEX_UNIT_COUNT ? _units[unit].lastUpdate : 0;
}

const ApexProbe* Apex::getProbes(uint8_t unit) {
  return unit < APEX_UNIT_COUNT ? _units[unit].probes : nullptr;
}

uint8_t Apex::probeCount(uint8_t unit) {
  return unit < APEX_UNIT_COUNT ? _units[unit].probeCount : 0;
}

// ── helpers ──────────────────────────────────────────

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

void Apex::_poll(uint8_t unitIdx) {
  UnitState& unit = _units[unitIdx];
  WiFiClient client;
  String path = "/cgi-bin/status.xml";
  String host = String(unit.config.ip);
  String auth = String(unit.config.username) + ":" + String(unit.config.password);
  String authB64 = _base64Encode((const uint8_t*)auth.c_str(), auth.length());

  if (!client.connect(unit.config.ip, unit.config.port, APEX_TIMEOUT_MS)) {
    unit.connected = false;
    Logger::warn(String(F("Apex ")) + unitIdx + F(": connection failed"));
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
    unit.connected = false;
    Logger::warn(String(F("Apex ")) + unitIdx + F(": empty response"));
    return;
  }

  int spaceIdx = resp.indexOf(' ');
  if (spaceIdx < 0) { unit.connected = false; return; }
  String statusCode = resp.substring(spaceIdx + 1, spaceIdx + 4);

  if (statusCode != "200") {
    Logger::warn(String(F("Apex ")) + unitIdx + F(": HTTP ") + statusCode);
    unit.connected = false;
    return;
  }

  int bodyStart = resp.indexOf("\r\n\r\n");
  if (bodyStart < 0) bodyStart = resp.indexOf("\n\n");
  if (bodyStart < 0) { unit.connected = false; return; }
  bodyStart += (resp[bodyStart + 1] == '\n' ? 2 : 4);
  String body = resp.substring(bodyStart);
  body.trim();

  // parse XML probes
  unit.probeCount = 0;
  int pos = 0;
  while (unit.probeCount < APEX_MAX_PROBES) {
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

    ApexProbe& probe = unit.probes[unit.probeCount];
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

    unit.probeCount++;
  }

  unit.connected = (unit.probeCount > 0);
  unit.lastUpdate = millis();

  if (unit.probeCount > 0) {
    Logger::info(String(F("Apex ")) + unitIdx + F(": ") + unit.probeCount + F(" probes OK"));
  } else {
    Logger::warn(String(F("Apex ")) + unitIdx + F(": no <probe> elements found"));
  }
}
