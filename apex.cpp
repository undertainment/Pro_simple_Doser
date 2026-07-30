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

// ── helpers ──────────────────────────────────────────

static String _md5(const String& in) {
  MD5Builder md5;
  md5.begin();
  md5.add(in);
  md5.calculate();
  return md5.toString();
}

static int _readResponse(WiFiClient& c, String& out, unsigned long timeoutMs) {
  unsigned long deadline = millis() + timeoutMs;
  out = "";
  while (millis() < deadline) {
    while (c.available()) {
      out += (char)c.read();
    }
    // headers + body fully received when connection closes (Connection: close)
    if (!c.connected() && !c.available()) break;
    delay(2);
  }
  return out.length();
}

// ── poll ─────────────────────────────────────────────

void Apex::_poll() {
  WiFiClient client;
  String path = "/status.xml";
  String host = String(_config.ip);

  Logger::info(String(F("Apex: connecting to ")) + host + ":" + String(_config.port));

  if (!client.connect(_config.ip, _config.port, APEX_TIMEOUT_MS)) {
    _connected = false;
    Logger::warn(F("Apex: connection failed (unreachable)"));
    return;
  }

  // --- first request (no auth) ---
  String req = "GET " + path + " HTTP/1.1\r\n"
               "Host: " + host + "\r\n"
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

  // --- handle 401 digest challenge ---
  if (statusCode == "401") {
    int authStart = resp.indexOf("WWW-Authenticate:");
    if (authStart < 0) authStart = resp.indexOf("www-authenticate:");
    if (authStart < 0) {
      Logger::warn(F("Apex: 401 but no WWW-Authenticate header"));
      _connected = false;
      return;
    }
    int authEnd = resp.indexOf('\r', authStart);
    if (authEnd < 0) authEnd = resp.indexOf('\n', authStart);
    if (authEnd < 0) authEnd = resp.length();
    String authHeader = resp.substring(authStart, authEnd);

    String realm, nonce, qop;
    int r = authHeader.indexOf("realm=\"");
    if (r >= 0) { r += 7; int e = authHeader.indexOf('"', r); if (e > r) realm = authHeader.substring(r, e); }
    int n = authHeader.indexOf("nonce=\"");
    if (n >= 0) { n += 7; int e = authHeader.indexOf('"', n); if (e > n) nonce = authHeader.substring(n, e); }
    int q = authHeader.indexOf("qop=\"");
    if (q >= 0) { q += 5; int e = authHeader.indexOf('"', q); if (e > q) qop = authHeader.substring(q, e); }
    bool hasQop = (qop.length() > 0);

    if (realm.length() == 0 || nonce.length() == 0) {
      Logger::warn(F("Apex: digest challenge missing realm/nonce"));
      _connected = false;
      return;
    }

    // Build digest response
    String ha1 = _md5(String(_config.username) + ":" + realm + ":" + String(_config.password));
    String ha2 = _md5(String("GET") + ":" + path);
    String digestResp;

    String authLine = "Digest username=\"" + String(_config.username) + "\", "
                      "realm=\"" + realm + "\", "
                      "nonce=\"" + nonce + "\", "
                      "uri=\"" + path + "\"";

    if (hasQop) {
      String cnonce = "abc123";
      String nc = "00000001";
      digestResp = _md5(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
      authLine += ", qop=" + qop + ", nc=" + nc + ", cnonce=\"" + cnonce + "\"";
    } else {
      digestResp = _md5(ha1 + ":" + nonce + ":" + ha2);
    }
    authLine += ", response=\"" + digestResp + "\"";

    // --- second request (with auth) ---
    if (!client.connect(_config.ip, _config.port, APEX_TIMEOUT_MS)) {
      _connected = false;
      Logger::warn(F("Apex: reconnect failed"));
      return;
    }

    req = "GET " + path + " HTTP/1.1\r\n"
          "Host: " + host + "\r\n"
          "Authorization: " + authLine + "\r\n"
          "Connection: close\r\n\r\n";
    client.print(req);

    resp = "";
    _readResponse(client, resp, APEX_TIMEOUT_MS);
    client.stop();

    if (resp.length() == 0) {
      _connected = false;
      Logger::warn(F("Apex: empty response after auth"));
      return;
    }

    spaceIdx = resp.indexOf(' ');
    if (spaceIdx < 0) { _connected = false; return; }
    statusCode = resp.substring(spaceIdx + 1, spaceIdx + 4);
  }

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

  Logger::info(String(F("Apex: body length ")) + body.length() + F(" bytes"));

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
    else if (pname == "Temp" || pname == "Temperature") strcpy(probe.label, "Temp");
    else if (pname == "ORP") strcpy(probe.label, "ORP");
    else if (pname == "Salinity" || pname == "Sal") strcpy(probe.label, "Salinity");
    else if (pname == "ALK" || pname == "Alkalinity") strcpy(probe.label, "Alk");
    else if (pname == "Ca" || pname == "Calcium") strcpy(probe.label, "Calcium");
    else if (pname == "Mg" || pname == "Magnesium") strcpy(probe.label, "Magnesium");
    else strncpy(probe.label, pname.c_str(), sizeof(probe.label) - 1);

    _probeCount++;
  }

  _connected = (_probeCount > 0);
  _lastUpdate = millis();

  if (_probeCount > 0) {
    Logger::info(String(F("Apex Classic: ")) + _probeCount + F(" probes read OK"));
  } else {
    Logger::warn(F("Apex Classic: no <probe> elements found in XML"));
  }
}
