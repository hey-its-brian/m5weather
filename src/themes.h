#pragma once

#include <Arduino.h>
#include <M5GFX.h>

// A theme is a named palette for the display renderer. The Spectra 6 panel
// physically shows black, white, red, yellow, blue, and green; M5GFX dithers
// any RGB color to that palette, so themes stick to the pure colors.
struct Theme {
  const char *id;      // stable key stored in config
  const char *label;   // shown in the web UI
  uint32_t bg;
  uint32_t text;       // primary text
  uint32_t subtext;    // secondary text (dithers to gray)
  uint32_t accent;     // header rules, highlights
  uint32_t tempHi;     // daily high temps
  uint32_t tempLo;     // daily low temps / precipitation
  uint32_t sun;
  uint32_t cloud;
  uint32_t rain;
};

const Theme *themeById(const String &id);   // falls back to the first theme
const Theme *themeList(size_t &count);
