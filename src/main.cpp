// M5Weather: weather dashboard for the M5Stack PaperColor.
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
#include <esp_sntp.h>

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

// The battery-backed RTC can hold a plausible-but-wrong time, so "epoch looks
// recent" is not proof of sync; only the SNTP callback is.
static volatile bool g_timeSynced = false;
static bool g_timeSyncHandled = false;

// Correct the hardware RTC so future boots start with a sane clock.
static void syncRtcFromSystemTime() {
  if (!M5.Rtc.isEnabled()) return;
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  M5.Rtc.setDateTime(&utc);
  Serial.println("[m5weather] hardware RTC set from NTP");
}

// WPA2 on the setup hotspot so passers-by can't inject WiFi credentials
// during the setup window. The password is shown on the e-ink screen.
static const char *AP_PASS = "m5weather";

static void startCaptivePortal(const String &reason) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  webServerStart(/*captivePortal=*/true);
  renderStatus("Setup needed",
               reason + "\n\n"
               "1. Join Wi-Fi network:  " + AP_SSID + "\n"
               "    (password:  " + AP_PASS + ")\n"
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
  delay(2000);  // give USB CDC time to enumerate so early logs are visible
  Serial.printf("[m5weather] board=%d display=%dx%d psram=%u heap=%u\n",
                (int)M5.getBoard(), M5.Display.width(), M5.Display.height(),
                ESP.getPsramSize(), ESP.getFreeHeap());

  displayInit();
  config.load();
  Serial.printf("[m5weather] wifi_ssid='%s' zip='%s' theme='%s'\n",
                config.wifiSsid.c_str(), config.zip.c_str(), config.theme.c_str());

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
  sntp_set_time_sync_notification_cb([](struct timeval *) { g_timeSynced = true; });
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC; offset comes from the API

  // Wait briefly for the first NTP sync so the initial render has a clock.
  unsigned long ntpStart = millis();
  while (!g_timeSynced && millis() - ntpStart < 15000) delay(100);
  Serial.printf("[m5weather] ntp synced=%d after %lums\n",
                (int)g_timeSynced, millis() - ntpStart);
  if (g_timeSynced) {          // synced in time: nothing to redo later
    syncRtcFromSystemTime();
    g_timeSyncHandled = true;
  }

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

  // First NTP sync: correct the hardware RTC for future boots, and redo any
  // timestamp/render taken while the clock was still wrong.
  if (g_timeSynced && !g_timeSyncHandled) {
    g_timeSyncHandled = true;
    syncRtcFromSystemTime();
    if (weather.valid) {
      weather.fetchedAt = time(nullptr);  // fetch completed moments ago
      g_renderRequested = true;
    }
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
