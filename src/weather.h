#pragma once

#include <Arduino.h>

static const int FORECAST_DAYS = 5;

struct DailyForecast {
  int weekday = 0;        // 0=Sun .. 6=Sat
  int code = 0;           // WMO weather code
  float tempMax = 0;
  float tempMin = 0;
  int precipProb = 0;     // percent
};

struct WeatherData {
  bool valid = false;

  // Current conditions
  float temperature = 0;
  float feelsLike = 0;
  int humidity = 0;       // percent
  float windSpeed = 0;
  int code = 0;           // WMO weather code
  bool isDay = true;

  long utcOffsetSeconds = 0;   // location's UTC offset, from the API
  time_t fetchedAt = 0;        // UTC epoch of the fetch

  DailyForecast daily[FORECAST_DAYS];
};

extern WeatherData weather;

// Resolve a US zip to lat/lon + place name (Zippopotam.us, no API key).
// Updates config (not persisted — caller saves). Returns false on failure.
bool geocodeZip(const String &zip, String &errorOut);

// Fetch current + 5-day forecast from Open-Meteo (no API key).
bool fetchWeather(String &errorOut);

// Human-readable label for a WMO weather code, e.g. "Partly cloudy".
const char *wmoDescription(int code);
