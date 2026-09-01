#include "portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <M5Unified.h>
#include <time.h>

#include "config.h"
#include "sensor.h"
#include "themes.h"
#include "weather.h"
#include "webui.h"

// Set by handlers, consumed by the main loop (rendering the e-ink display
// takes seconds, so it must not happen inside an HTTP handler).
extern volatile bool g_renderRequested;
extern volatile bool g_fetchRequested;
extern volatile bool g_rebootRequested;

static WebServer server(80);
static DNSServer dns;
static bool captiveMode = false;

static void sendJson(int status, const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(status, "application/json", out);
}

static void sendError(int status, const String &message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(status, doc);
}

// --- Request origin checks -------------------------------------------------
// The API has no auth (trusted-LAN model), so at minimum reject requests that
// a browser was tricked into sending: DNS rebinding (Host header points at a
// foreign name that resolved to us) and cross-site POSTs (foreign Origin).

static bool hostMatches(String h) {
  int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  h.toLowerCase();
  if (h == "m5weather.local" || h == "m5weather") return true;
  if (WiFi.status() == WL_CONNECTED && h == WiFi.localIP().toString()) return true;
  if (h == WiFi.softAPIP().toString()) return true;
  return false;
}

static bool requestAllowed() {
  if (!hostMatches(server.hostHeader())) return false;
  String origin = server.header("Origin");
  if (origin.length()) {  // absent for same-origin GETs and curl; strict if present
    if (!origin.startsWith("http://")) return false;
    if (!hostMatches(origin.substring(7))) return false;
  }
  return true;
}

// Returns false (after sending a response) if the request must be rejected.
static bool guard() {
  if (requestAllowed()) return true;
  server.send(403, "text/plain", "Forbidden");
  return false;
}

static void handleRoot() {
  if (!requestAllowed()) {
    if (captiveMode) {  // captive-portal probes carry foreign Hosts: redirect
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(403, "text/plain", "Forbidden");
    }
    return;
  }
  server.send_P(200, "text/html", WEBUI_HTML);
}

static void handleGetConfig() {
  if (!guard()) return;
  JsonDocument doc;
  doc["zip"] = config.zip;
  doc["units"] = config.units;
  doc["refresh_minutes"] = config.refreshMinutes;
  doc["theme"] = config.theme;
  doc["wifi_ssid"] = config.wifiSsid;

  JsonArray themes = doc["themes"].to<JsonArray>();
  size_t count;
  const Theme *list = themeList(count);
  for (size_t i = 0; i < count; i++) {
    JsonObject t = themes.add<JsonObject>();
    t["id"] = list[i].id;
    t["label"] = list[i].label;
  }
  sendJson(200, doc);
}

static void handlePostConfig() {
  if (!guard()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendError(400, "Invalid JSON");
    return;
  }

  String zip = doc["zip"] | "";
  bool zipOk = zip.length() == 5;
  for (size_t i = 0; zipOk && i < zip.length(); i++) {
    if (!isDigit(zip[i])) zipOk = false;
  }
  if (!zipOk) {
    sendError(400, "Zip code must be 5 digits");
    return;
  }

  // Resolve the zip before committing, so a typo doesn't wipe a working
  // location. Requires internet, which captive-portal mode doesn't have.
  bool locationChanged = zip != config.zip || !config.hasLocation();
  if (locationChanged) {
    if (WiFi.status() != WL_CONNECTED) {
      sendError(503, "Not connected to Wi-Fi yet; set Wi-Fi first");
      return;
    }
    String err;
    if (!geocodeZip(zip, err)) {
      sendError(422, err);
      return;
    }
  }

  config.units = doc["units"] | config.units;
  config.refreshMinutes = doc["refresh_minutes"] | config.refreshMinutes;
  if (config.refreshMinutes < 5) config.refreshMinutes = 5;
  config.theme = doc["theme"] | config.theme;
  config.save();

  // A new location (or no data yet) needs a fetch; otherwise just redraw.
  if (locationChanged || !weather.valid) g_fetchRequested = true;
  else g_renderRequested = true;

  JsonDocument res;
  res["ok"] = true;
  res["place"] = config.placeName;
  sendJson(200, res);
}

static void handlePostWifi() {
  if (!guard()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendError(400, "Invalid JSON");
    return;
  }
  String ssid = doc["ssid"] | "";
  if (!ssid.length()) {
    sendError(400, "SSID required");
    return;
  }
  // Empty password field means "keep the existing one" unless SSID changed.
  String pass = doc["pass"] | "";
  if (pass.length() || ssid != config.wifiSsid) config.wifiPass = pass;
  config.wifiSsid = ssid;
  config.save();

  JsonDocument res;
  res["ok"] = true;
  sendJson(200, res);
  delay(200);  // let the response flush before rebooting
  g_rebootRequested = true;
}

static void handlePostRefresh() {
  if (!guard()) return;
  if (WiFi.status() != WL_CONNECTED) {
    sendError(503, "Not connected to Wi-Fi");
    return;
  }
  String err;
  if (!fetchWeather(err)) {
    sendError(502, err);
    return;
  }
  g_renderRequested = true;
  JsonDocument res;
  res["ok"] = true;
  sendJson(200, res);
}

static void handleStatus() {
  if (!guard()) return;
  JsonDocument doc;
  doc["place"] = config.placeName;

  if (weather.valid && weather.fetchedAt > 0) {
    time_t local = weather.fetchedAt + weather.utcOffsetSeconds;
    struct tm lt;
    gmtime_r(&local, &lt);
    char buf[24];
    int h12 = lt.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
    doc["last_update"] = buf;
  }

  if (room.valid) {
    doc["room_temp_c"] = roundf(room.tempC * 10) / 10;
    doc["room_humidity"] = (int)roundf(room.humidity);
  }

  int32_t batt = M5.Power.getBatteryLevel();
  doc["battery_pct"] = batt;  // -1 if unknown
  if (WiFi.status() == WL_CONNECTED) {
    doc["rssi"] = WiFi.RSSI();
    doc["address"] = "http://m5weather.local (" + WiFi.localIP().toString() + ")";
  } else if (captiveMode) {
    doc["address"] = "http://" + WiFi.softAPIP().toString();
  }
  sendJson(200, doc);
}

void webServerStart(bool captivePortal) {
  captiveMode = captivePortal;

  static const char *COLLECT_HEADERS[] = {"Origin"};
  server.collectHeaders(COLLECT_HEADERS, 1);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/wifi", HTTP_POST, handlePostWifi);
  server.on("/api/refresh", HTTP_POST, handlePostRefresh);
  server.on("/api/status", HTTP_GET, handleStatus);

  if (captivePortal) {
    // Answer every DNS query with our AP address so phones/laptops pop the
    // portal page automatically.
    dns.start(53, "*", WiFi.softAPIP());
    server.onNotFound([]() {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    });
  } else {
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  }

  server.begin();
}

void webServerLoop() {
  if (captiveMode) dns.processNextRequest();
  server.handleClient();
}
