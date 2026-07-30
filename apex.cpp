#include "apex.h"
#include "logger.h"
#include <WiFi.h>
#include <MD5Builder.h>

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
  Logger::info(F("Apex client initialized"));
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

static String _extractHeader(const String& response, const String& header) {
  int start = response.indexOf("\n" + header);
  if (start < 0) start = response.indexOf("\r" + header);
  if (start < 0) start = response.indexOf(header);
  if (start < 0) return "";
  int colon = response.indexOf(':', start);
  if (colon < 0) return "";
  int end = response.indexOf('\r', colon);
  if (end < 0) end = response.indexOf('\n', colon);
  if (end < 0) end = response.length();
  String val = response.substring(colon + 1, end);
  val.trim();
  return val;
}

static String _md5(const String& in) {
  MD5Builder md5;
  md5.begin();
  md5.add(in);
  md5.calculate();
  return md5.toString();
}

static String _digestAuthResponse(const String& user, const String& pass,
                                   const String& realm, const String& nonce,
                                   const String& qop, const String& method,
                                   const String& uri) {
  String ha1 = _md5(user + ":" + realm + ":" + pass);
  String ha2 = _md5(method + ":" + uri);
  String cnonce = "abc123";
  String nc = "00000001";
  String resp = _md5(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
  return resp;
}

void Apex::_poll() {
  WiFiClient client;

  if (!client.connect(_config.ip, _config.port, APEX_TIMEOUT_MS)) {
    _connected = false;
    Logger::warn(F("Apex: connection failed"));
    return;
  }

  String path = "/rest/probe";
  String req = "GET " + path + " HTTP/1.1\r\n"
               "Host: " + String(_config.ip) + ":" + String(_config.port) + "\r\n"
               "Connection: close\r\n\r\n";
  client.print(req);

  unsigned long timeout = millis() + APEX_TIMEOUT_MS;
  String resp;
  while (millis() < timeout) {
    while (client.available()) {
      resp += (char)client.read();
    }
    if (resp.indexOf("\r\n\r\n") >= 0 || resp.indexOf("\n\n") >= 0) break;
    delay(5);
  }

  client.stop();

  // Check for 401 digest challenge
  int spaceIdx = resp.indexOf(' ');
  if (spaceIdx < 0) { _connected = false; return; }
  String statusCode = resp.substring(spaceIdx + 1, spaceIdx + 4);

  if (statusCode == "401") {
    String authHeader = _extractHeader(resp, "WWW-Authenticate");
    if (authHeader.length() == 0) { _connected = false; return; }

    String realm, nonce, qop;
    int r = authHeader.indexOf("realm=\"");
    if (r >= 0) { r += 7; int e = authHeader.indexOf('"', r); realm = authHeader.substring(r, e); }
    int n = authHeader.indexOf("nonce=\"");
    if (n >= 0) { n += 7; int e = authHeader.indexOf('"', n); nonce = authHeader.substring(n, e); }
    int q = authHeader.indexOf("qop=\"");
    if (q >= 0) { q += 5; int e = authHeader.indexOf('"', q); qop = authHeader.substring(q, e); }
    if (qop.length() == 0) qop = "auth";

    String digestResp = _digestAuthResponse(_config.username, _config.password, realm, nonce, qop, "GET", path);
    String cnonce = "abc123";
    String nc = "00000001";

    if (!client.connect(_config.ip, _config.port, APEX_TIMEOUT_MS)) {
      _connected = false; return;
    }

    String authLine = "Digest username=\"" + String(_config.username) + "\", "
                      "realm=\"" + realm + "\", "
                      "nonce=\"" + nonce + "\", "
                      "uri=\"" + path + "\", "
                      "qop=" + qop + ", "
                      "nc=" + nc + ", "
                      "cnonce=\"" + cnonce + "\", "
                      "response=\"" + digestResp + "\"";

    req = "GET " + path + " HTTP/1.1\r\n"
          "Host: " + String(_config.ip) + ":" + String(_config.port) + "\r\n"
          "Authorization: " + authLine + "\r\n"
          "Connection: close\r\n\r\n";
    client.print(req);

    timeout = millis() + APEX_TIMEOUT_MS;
    resp = "";
    while (millis() < timeout) {
      while (client.available()) {
        resp += (char)client.read();
      }
      if (resp.indexOf("\r\n\r\n") >= 0 || resp.indexOf("\n\n") >= 0) break;
      delay(5);
    }
    client.stop();

    spaceIdx = resp.indexOf(' ');
    if (spaceIdx < 0) { _connected = false; return; }
    statusCode = resp.substring(spaceIdx + 1, spaceIdx + 4);
  }

  if (statusCode != "200") {
    _connected = false;
    return;
  }

  // Extract body
  int bodyStart = resp.indexOf("\r\n\r\n");
  if (bodyStart < 0) bodyStart = resp.indexOf("\n\n");
  if (bodyStart < 0) { _connected = false; return; }
  bodyStart += (resp[bodyStart + 1] == '\n' ? 2 : 4);
  String body = resp.substring(bodyStart);
  body.trim();

  // Parse JSON probe array: [{"id":"Px1","name":"pH","value":"8.12","type":"pH"},...]
  _probeCount = 0;
  int pos = 0;
  while (_probeCount < APEX_MAX_PROBES) {
    int openBrace = body.indexOf('{', pos);
    if (openBrace < 0) break;
    int closeBrace = body.indexOf('}', openBrace);
    if (closeBrace < 0) break;
    String obj = body.substring(openBrace + 1, closeBrace);
    pos = closeBrace + 1;

    // Extract name
    int ni = obj.indexOf("\"name\":\"");
    if (ni < 0) continue;
    ni += 8;
    int ne = obj.indexOf('"', ni);
    if (ne < 0) continue;
    String pname = obj.substring(ni, ne);
    pname.trim();

    // Extract value
    int vi = obj.indexOf("\"value\":\"");
    if (vi < 0) vi = obj.indexOf("\"value\":");
    if (vi < 0) continue;
    vi = obj.indexOf(':', vi) + 1;
    while (vi < (int)obj.length() && obj[vi] == '"') vi++;
    int ve = vi;
    while (ve < (int)obj.length() && obj[ve] != ',' && obj[ve] != '}') ve++;
    // backtrack past closing quote
    if (ve > vi && obj[ve - 1] == '"') ve--;
    String pval = obj.substring(vi, ve);
    pval.trim();

    ApexProbe& probe = _probes[_probeCount];
    strncpy(probe.name, pname.c_str(), sizeof(probe.name) - 1);
    probe.name[sizeof(probe.name) - 1] = '\0';
    probe.value = pval.toFloat();

    // Build a readable label
    if (pname == "pH") strcpy(probe.label, "pH");
    else if (pname == "Temp" || pname == "Temperature") strcpy(probe.label, "Temp");
    else if (pname == "ORP") strcpy(probe.label, "ORP");
    else if (pname == "Salinity" || pname == "Sal") strcpy(probe.label, "Salinity");
    else if (pname == "ALK" || pname == "Alkalinity") strcpy(probe.label, "Alk");
    else if (pname == "Ca" || pname == "Calcium") strcpy(probe.label, "Calcium");
    else if (pname == "Mg" || pname == "Magnesium") strcpy(probe.label, "Magnesium");
    else strncpy(probe.label, pname.c_str(), sizeof(probe.label) - 1);

    _probeCount++;
  }

  _connected = _probeCount > 0;
  _lastUpdate = millis();
  Logger::info(String(F("Apex: fetched ")) + _probeCount + F(" probes"));
}
