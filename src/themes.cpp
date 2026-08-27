#include "themes.h"

// The panel driver's exact palette entries (Panel_ED2208). With dithering off
// (epd_fastest), colors must match these to land on the intended ink.
static constexpr uint32_t C_WHITE  = 0xFFFFFFu;
static constexpr uint32_t C_BLACK  = 0x000000u;
static constexpr uint32_t C_RED    = 0xBF0000u;
static constexpr uint32_t C_YELLOW = 0xFFF338u;
static constexpr uint32_t C_BLUE   = 0x6440FFu;
static constexpr uint32_t C_GREEN  = 0x438A1Cu;

static const Theme THEMES[] = {
    {
        .id = "classic",
        .label = "Classic",
        .bg = C_WHITE,
        .text = C_BLACK,
        .subtext = C_BLACK,
        .accent = C_RED,
        .tempHi = C_RED,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_WHITE,   // white fill + black outline reads as a cloud
        .rain = C_BLUE,
    },
    {
        .id = "night",
        .label = "Night",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_WHITE,
        .accent = C_YELLOW,
        .tempHi = C_YELLOW,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_WHITE,
        .rain = C_BLUE,
    },
    {
        .id = "forest",
        .label = "Forest",
        .bg = C_WHITE,
        .text = C_BLACK,
        .subtext = C_BLACK,
        .accent = C_GREEN,
        .tempHi = C_GREEN,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_WHITE,
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
