// M5Weather — weather dashboard for the M5Stack PaperColor.
//
// Boot flow:
//   - With no Wi-Fi configured (or connect failure): open the
//     "M5Weather-Setup" hotspot with a captive config portal.
//   - Otherwise: join Wi-Fi, sync NTP, geocode the configured zip if needed,
//     fetch weather from Open-Meteo, render, and serve the config UI at
//     http://m5weather.local for the rest of the session.

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "weather.h"
#include "portal.h"

static const char *AP_SSID = "M5Weather-Setup";
static const char *MDNS_NAME = "m5weather";

volatile bool g_renderRequested = false;
volatile bool g_rebootRequested = false;

static bool staConnected = false;
static unsigned long lastFetchMs = 0;
static uint8_t consecutiveFailures = 0;

static void startCaptivePortal(const String &reason) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  webServerStart(/*captivePortal=*/true);
  renderStatus("Setup needed",
               reason + "\n\n"
               "1. Join Wi-Fi network:  " + AP_SSID + "\n"
               "2. Open:  http://" + WiFi.softAPIP().toString() + "\n"
               "3. Enter your Wi-Fi and zip code");
}

static bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

static void fetchAndRender() {
  String err;
  if (!config.hasLocation() && config.zip.length() == 5) {
    if (!geocodeZip(config.zip, err)) {
      renderStatus("Location error", err + "\nFix the zip at http://m5weather.local");
      return;
    }
    config.save();
  }
  if (!config.hasLocation()) {
    renderStatus("Almost there",
                 "Connected to Wi-Fi.\n"
                 "Set your zip code at:\nhttp://m5weather.local");
    return;
  }

  if (fetchWeather(err)) {
    consecutiveFailures = 0;
    renderWeather();
  } else {
    Serial.printf("Weather fetch failed: %s\n", err.c_str());
    // Keep showing the last good data; only alarm after repeated failures.
    if (++consecutiveFailures >= 3 && !weather.valid) {
      renderStatus("Weather unavailable", err);
    }
  }
  lastFetchMs = millis();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  displayInit();
  config.load();

  if (!config.hasWifi()) {
    startCaptivePortal("Welcome! Let's get connected.");
    return;
  }

  renderStatus("Starting up", "Connecting to " + config.wifiSsid + "...");
  if (!connectWifi()) {
    startCaptivePortal("Couldn't join \"" + config.wifiSsid + "\".");
    return;
  }
  staConnected = true;

  MDNS.begin(MDNS_NAME);
  MDNS.addService("http", "tcp", 80);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC; offset comes from the API

  webServerStart(/*captivePortal=*/false);
  fetchAndRender();
}

void loop() {
  M5.update();
  webServerLoop();

  if (g_rebootRequested) {
    delay(500);
    ESP.restart();
  }

  if (g_renderRequested) {
    g_renderRequested = false;
    renderWeather();
  }

  if (staConnected) {
    // Periodic weather refresh.
    unsigned long interval = (unsigned long)config.refreshMinutes * 60000UL;
    if (millis() - lastFetchMs >= interval) {
      if (WiFi.status() == WL_CONNECTED) {
        fetchAndRender();
      } else {
        WiFi.reconnect();
        lastFetchMs = millis() - interval + 60000UL;  // retry in a minute
      }
    }
  }

  delay(10);
}
