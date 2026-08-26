#include "themes.h"

// Pure Spectra 6 colors — anything else gets dithered by M5GFX.
static constexpr uint32_t C_WHITE  = 0xFFFFFFu;
static constexpr uint32_t C_BLACK  = 0x000000u;
static constexpr uint32_t C_RED    = 0xFF0000u;
static constexpr uint32_t C_YELLOW = 0xFFFF00u;
static constexpr uint32_t C_BLUE   = 0x0000FFu;
static constexpr uint32_t C_GREEN  = 0x008000u;
static constexpr uint32_t C_GRAY   = 0x808080u;  // dithered black/white

static const Theme THEMES[] = {
    {
        .id = "classic",
        .label = "Classic",
        .bg = C_WHITE,
        .text = C_BLACK,
        .subtext = C_GRAY,
        .accent = C_RED,
        .tempHi = C_RED,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_GRAY,
        .rain = C_BLUE,
    },
    {
        .id = "night",
        .label = "Night",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_GRAY,
        .accent = C_YELLOW,
        .tempHi = C_YELLOW,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_GRAY,
        .rain = C_BLUE,
    },
    {
        .id = "forest",
        .label = "Forest",
        .bg = C_WHITE,
        .text = C_BLACK,
        .subtext = C_GRAY,
        .accent = C_GREEN,
        .tempHi = C_GREEN,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_GRAY,
        .rain = C_BLUE,
    },
};

const Theme *themeById(const String &id) {
  for (const auto &t : THEMES) {
    if (id.equals(t.id)) return &t;
  }
  return &THEMES[0];
}

const Theme *themeList(size_t &count) {
  count = sizeof(THEMES) / sizeof(THEMES[0]);
  return THEMES;
}
