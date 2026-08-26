#pragma once

#include <Arduino.h>

// Persistent app settings, backed by NVS (Preferences).
struct AppConfig {
  // WiFi
  String wifiSsid;
  String wifiPass;

  // Location
  String zip;          // US zip code
  float  latitude  = 0;
  float  longitude = 0;
  String placeName;    // resolved from zip, e.g. "Beverly Hills, CA"

  // Behavior
  String units = "imperial";   // "imperial" | "metric"
  uint16_t refreshMinutes = 30;
  String theme = "classic";

  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasLocation() const { return latitude != 0 || longitude != 0; }

  void load();
  void save() const;
};

extern AppConfig config;
