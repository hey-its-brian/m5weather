#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"

WeatherData weather;

// HTTPS without certificate pinning: neither request carries anything
// sensitive (a zip code and coordinates), and skipping the CA bundle keeps
// the firmware free of certificate-expiry maintenance.

static bool httpGetJson(const String &url, JsonDocument &doc, String &errorOut) {
  NetworkClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  // HTTP/1.0 forbids chunked encoding (which getStream() can't decode and
  // which hides the response size), so the length check below is reliable.
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    errorOut = "http.begin failed";
    return false;
  }
  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    errorOut = "HTTP " + String(status) + " from " + url;
    http.end();
    return false;
  }
  if (http.getSize() > 65536) {  // don't let a hostile server eat the heap
    errorOut = "Response too large";
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    errorOut = String("JSON parse: ") + err.c_str();
    return false;
  }
  return true;
}

bool geocodeZip(const String &zip, String &errorOut) {
  JsonDocument doc;
  if (!httpGetJson("https://api.zippopotam.us/us/" + zip, doc, errorOut)) {
    if (errorOut.startsWith("HTTP 404")) errorOut = "Zip code not found: " + zip;
    return false;
  }

  JsonObject place = doc["places"][0];
  if (place.isNull()) {
    errorOut = "No places in geocode response";
    return false;
  }

  const char *lat = place["latitude"] | (const char *)nullptr;
  const char *lon = place["longitude"] | (const char *)nullptr;
  if (!lat || !lon) {
    errorOut = "Geocode response missing coordinates";
    return false;
  }
  config.latitude  = atof(lat);
  config.longitude = atof(lon);
  config.placeName = String(place["place name"] | "") + ", " +
                     (place["state abbreviation"] | "");
  config.zip = zip;
  return true;
}

bool fetchWeather(String &errorOut) {
  if (!config.hasLocation()) {
    errorOut = "No location configured";
    return false;
  }

  String url = "https://api.open-meteo.com/v1/forecast";
  url += "?latitude=" + String(config.latitude, 4);
  url += "&longitude=" + String(config.longitude, 4);
  url += "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
         "weather_code,wind_speed_10m,is_day";
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
         "precipitation_probability_max";
  url += "&forecast_days=" + String(FORECAST_DAYS);
  url += "&timezone=auto";
  if (config.units == "imperial") {
    url += "&temperature_unit=fahrenheit&wind_speed_unit=mph";
  }

  JsonDocument doc;
  if (!httpGetJson(url, doc, errorOut)) return false;

  JsonObject current = doc["current"];
  if (current.isNull()) {
    errorOut = "Missing 'current' in forecast response";
    return false;
  }

  weather.temperature = current["temperature_2m"].as<float>();
  weather.feelsLike   = current["apparent_temperature"].as<float>();
  weather.humidity    = current["relative_humidity_2m"].as<int>();
  weather.windSpeed   = current["wind_speed_10m"].as<float>();
  weather.code        = current["weather_code"].as<int>();
  weather.isDay       = current["is_day"].as<int>() == 1;

  weather.utcOffsetSeconds = doc["utc_offset_seconds"].as<long>();
  weather.fetchedAt = time(nullptr);

  JsonObject daily = doc["daily"];
  for (int i = 0; i < FORECAST_DAYS; i++) {
    DailyForecast &d = weather.daily[i];
    d.code       = daily["weather_code"][i].as<int>();
    d.tempMax    = daily["temperature_2m_max"][i].as<float>();
    d.tempMin    = daily["temperature_2m_min"][i].as<float>();
    d.precipProb = daily["precipitation_probability_max"][i].as<int>();

    // Parse "YYYY-MM-DD" into a weekday for the forecast labels.
    const char *dateStr = daily["time"][i].as<const char *>();
    if (dateStr) {
      struct tm tmDay = {};
      tmDay.tm_year = atoi(dateStr) - 1900;
      tmDay.tm_mon  = atoi(dateStr + 5) - 1;
      tmDay.tm_mday = atoi(dateStr + 8);
      tmDay.tm_hour = 12;
      time_t t = mktime(&tmDay);
      struct tm out;
      localtime_r(&t, &out);
      d.weekday = out.tm_wday;
    }
  }

  weather.valid = true;
  return true;
}

const char *wmoDescription(int code) {
  switch (code) {
    case 0: return "Clear sky";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunderstorm w/ hail";
    default: return "Unknown";
  }
}
