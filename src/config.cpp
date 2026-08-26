#include "config.h"

#include <Preferences.h>

AppConfig config;

static const char *NVS_NAMESPACE = "m5weather";

void AppConfig::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  wifiSsid       = prefs.getString("wifi_ssid", "");
  wifiPass       = prefs.getString("wifi_pass", "");
  zip            = prefs.getString("zip", "");
  latitude       = prefs.getFloat("lat", 0);
  longitude      = prefs.getFloat("lon", 0);
  placeName      = prefs.getString("place", "");
  units          = prefs.getString("units", "imperial");
  refreshMinutes = prefs.getUShort("refresh_min", 30);
  theme          = prefs.getString("theme", "classic");
  prefs.end();

  if (refreshMinutes < 5) refreshMinutes = 5;
}

void AppConfig::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putString("wifi_ssid", wifiSsid);
  prefs.putString("wifi_pass", wifiPass);
  prefs.putString("zip", zip);
  prefs.putFloat("lat", latitude);
  prefs.putFloat("lon", longitude);
  prefs.putString("place", placeName);
  prefs.putString("units", units);
  prefs.putUShort("refresh_min", refreshMinutes);
  prefs.putString("theme", theme);
  prefs.end();
}
