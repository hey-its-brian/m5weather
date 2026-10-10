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
    {
        .id = "comic",
        .label = "Comic",
        .bg = C_WHITE,
        .text = C_BLACK,
        .subtext = C_BLACK,
        .accent = C_RED,
        .tempHi = C_RED,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_WHITE,
        .rain = C_BLUE,
        .style = STYLE_COMIC,
    },
    {
        // Colors map to the movie prop: red destination row, green present
        // row, yellow departed row, white labels, all on black.
        .id = "circuit",
        .label = "Time Circuit",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_WHITE,
        .accent = C_GREEN,
        .tempHi = C_RED,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_WHITE,
        .rain = C_BLUE,
        .style = STYLE_CIRCUIT,
    },
    // DECK//OS: the Tab5 cyberdeck's HUD, translated to the six inks. The
    // deck's four accents map by hue: cyan -> blue, magenta -> red, acid
    // yellow -> yellow, phosphor green -> green. `subtext` plays the deck's
    // dim blue-gray; it is blue where blue is free, otherwise white (labels
    // then rely on the thinner mono face for hierarchy). Icons are drawn in
    // the accent, so sun/cloud/rain mirror accent/bg/accent2.
    {
        .id = "deck_netrunner",
        .label = "DECK//OS Netrunner",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_WHITE,
        .accent = C_BLUE,
        .tempHi = C_WHITE,
        .tempLo = C_WHITE,
        .sun = C_BLUE,
        .cloud = C_BLACK,
        .rain = C_RED,
        .accent2 = C_RED,
        .grid = C_BLUE,
        .style = STYLE_DECK,
    },
    {
        .id = "deck_arasaka",
        .label = "DECK//OS Arasaka",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_WHITE,
        .accent = C_RED,
        .tempHi = C_WHITE,
        .tempLo = C_WHITE,
        .sun = C_RED,
        .cloud = C_BLACK,
        .rain = C_BLUE,
        .accent2 = C_BLUE,
        .grid = C_BLUE,
        .style = STYLE_DECK,
    },
    {
        .id = "deck_nomad",
        .label = "DECK//OS Nomad",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_BLUE,
        .accent = C_YELLOW,
        .tempHi = C_WHITE,
        .tempLo = C_BLUE,
        .sun = C_YELLOW,
        .cloud = C_BLACK,
        .rain = C_RED,
        .accent2 = C_RED,
        .grid = C_BLUE,
        .style = STYLE_DECK,
    },
    {
        .id = "deck_militech",
        .label = "DECK//OS Militech",
        .bg = C_BLACK,
        .text = C_WHITE,
        .subtext = C_BLUE,
        .accent = C_GREEN,
        .tempHi = C_WHITE,
        .tempLo = C_BLUE,
        .sun = C_GREEN,
        .cloud = C_BLACK,
        .rain = C_YELLOW,
        .accent2 = C_YELLOW,
        .grid = C_BLUE,
        .style = STYLE_DECK,
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
