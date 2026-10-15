#include "display.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>
#include <initializer_list>

#include "config.h"
#include "sensor.h"
#include "themes.h"
#include "weather.h"

// Set when the user toggles to the alternate view with a hardware button.
extern volatile bool g_altView;

// The theme for the currently selected view.
static const Theme *activeTheme() {
  if (g_altView && config.altTheme.length()) return themeById(config.altTheme);
  return themeById(config.theme);
}

// Landscape: 600 wide x 400 tall after rotation.
static const int W = 600;
static const int H = 400;

static M5GFX &gfx() { return M5.Display; }

void displayInit() {
  gfx().setRotation(1);           // portrait panel -> landscape
  // epd_fastest = nearest-color lookup with NO dithering. The other modes
  // Bayer-dither every pixel, so even exact palette colors pick up noise dots.
  // Flat UI colors that match the panel palette exactly render crisp.
  gfx().setEpdMode(epd_mode_t::epd_fastest);
  gfx().setTextWrap(false);
}

// ---------------------------------------------------------------------------
// Weather icons, drawn with primitives so they scale and follow theme colors.
// `cx, cy` is the icon center, `r` roughly half the icon's width.
// ---------------------------------------------------------------------------

static void drawSun(int cx, int cy, int r, const Theme *t) {
  gfx().fillCircle(cx, cy, r * 0.55f, t->sun);
  gfx().drawCircle(cx, cy, r * 0.55f, t->text);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    int x1 = cx + cosf(a) * r * 0.72f, y1 = cy + sinf(a) * r * 0.72f;
    int x2 = cx + cosf(a) * r,         y2 = cy + sinf(a) * r;
    for (int off = -1; off <= 1; off++) {
      gfx().drawLine(x1 + off, y1, x2 + off, y2, t->sun);
      gfx().drawLine(x1, y1 + off, x2, y2 + off, t->sun);
    }
  }
}

static void drawMoon(int cx, int cy, int r, const Theme *t) {
  gfx().fillCircle(cx, cy, r * 0.6f, t->sun);
  gfx().fillCircle(cx + r * 0.35f, cy - r * 0.2f, r * 0.5f, t->bg);
  gfx().drawCircle(cx, cy, r * 0.6f, t->text);
}

static void drawCloudShape(int cx, int cy, int r, uint32_t fill, uint32_t outline) {
  // Three lobes over a flat-bottomed base.
  gfx().fillCircle(cx - r * 0.45f, cy + r * 0.05f, r * 0.38f, fill);
  gfx().fillCircle(cx + r * 0.05f, cy - r * 0.25f, r * 0.48f, fill);
  gfx().fillCircle(cx + r * 0.5f,  cy + r * 0.05f, r * 0.36f, fill);
  gfx().fillRect(cx - r * 0.45f, cy + r * 0.05f, r * 0.95f, r * 0.38f, fill);
  gfx().drawCircle(cx - r * 0.45f, cy + r * 0.05f, r * 0.38f, outline);
  gfx().drawCircle(cx + r * 0.05f, cy - r * 0.25f, r * 0.48f, outline);
  gfx().drawCircle(cx + r * 0.5f,  cy + r * 0.05f, r * 0.36f, outline);
}

static void drawRainDrops(int cx, int cy, int r, int n, const Theme *t) {
  for (int i = 0; i < n; i++) {
    int x = cx - r * 0.5f + i * (r / (float)(n - 1 > 0 ? n - 1 : 1));
    gfx().drawLine(x, cy, x - r * 0.12f, cy + r * 0.35f, t->rain);
    gfx().drawLine(x + 1, cy, x + 1 - r * 0.12f, cy + r * 0.35f, t->rain);
  }
}

static void drawSnowFlakes(int cx, int cy, int r, const Theme *t) {
  for (int i = 0; i < 3; i++) {
    int x = cx - r * 0.5f + i * r * 0.5f;
    int y = cy + r * 0.2f;
    gfx().drawLine(x - 4, y, x + 4, y, t->text);
    gfx().drawLine(x, y - 4, x, y + 4, t->text);
    gfx().drawLine(x - 3, y - 3, x + 3, y + 3, t->text);
    gfx().drawLine(x - 3, y + 3, x + 3, y - 3, t->text);
  }
}

static void drawBolt(int cx, int cy, int r, const Theme *t) {
  gfx().fillTriangle(cx - r * 0.05f, cy - r * 0.1f,
                     cx + r * 0.25f, cy - r * 0.1f,
                     cx - r * 0.1f,  cy + r * 0.25f, t->sun);
  gfx().fillTriangle(cx + r * 0.1f,  cy - r * 0.05f,
                     cx - r * 0.1f,  cy + r * 0.25f,
                     cx + r * 0.05f, cy + r * 0.5f, t->sun);
}

// Draw the icon for a WMO weather code.
static void drawWeatherIcon(int code, bool isDay, int cx, int cy, int r, const Theme *t) {
  auto sunOrMoon = [&](int x, int y, int rr) {
    if (isDay) drawSun(x, y, rr, t); else drawMoon(x, y, rr, t);
  };

  switch (code) {
    case 0:
    case 1:
      sunOrMoon(cx, cy, r);
      break;
    case 2:  // partly cloudy: sun peeking behind cloud
      sunOrMoon(cx - r * 0.35f, cy - r * 0.35f, r * 0.6f);
      drawCloudShape(cx + r * 0.1f, cy + r * 0.15f, r * 0.85f, t->cloud, t->text);
      break;
    case 3:
      drawCloudShape(cx, cy, r, t->cloud, t->text);
      break;
    case 45: case 48:  // fog
      drawCloudShape(cx, cy - r * 0.3f, r * 0.8f, t->cloud, t->text);
      for (int i = 0; i < 3; i++)
        gfx().drawLine(cx - r * 0.6f, cy + r * 0.25f + i * 8,
                       cx + r * 0.6f, cy + r * 0.25f + i * 8, t->subtext);
      break;
    case 51: case 53: case 55: case 56: case 57:  // drizzle
      drawCloudShape(cx, cy - r * 0.25f, r * 0.85f, t->cloud, t->text);
      drawRainDrops(cx, cy + r * 0.35f, r * 0.7f, 3, t);
      break;
    case 61: case 63: case 65: case 66: case 67:  // rain
    case 80: case 81: case 82:                    // showers
      drawCloudShape(cx, cy - r * 0.25f, r * 0.85f, t->cloud, t->text);
      drawRainDrops(cx, cy + r * 0.35f, r * 0.9f, 4, t);
      break;
    case 71: case 73: case 75: case 77: case 85: case 86:  // snow
      drawCloudShape(cx, cy - r * 0.25f, r * 0.85f, t->cloud, t->text);
      drawSnowFlakes(cx, cy + r * 0.35f, r * 0.8f, t);
      break;
    case 95: case 96: case 99:  // thunderstorm
      drawCloudShape(cx, cy - r * 0.3f, r * 0.85f, t->cloud, t->text);
      drawBolt(cx, cy + r * 0.4f, r, t);
      drawRainDrops(cx - r * 0.5f, cy + r * 0.3f, r * 0.3f, 2, t);
      break;
    default:
      drawCloudShape(cx, cy, r, t->cloud, t->text);
      break;
  }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// Draws a rounded temperature plus a degree ring (the GFX fonts have no "°"
// glyph). Alignment: 'L' = middle-left, 'R' = middle-right, 'C' = top-center.
// Uses the currently set font/size; returns the total width drawn.
static int degreeSpace() { return (gfx().fontHeight() / 10 + 1) * 2 + 3; }

// Width drawTempValue would use for `v` in the current font, without drawing.
static int tempValueWidth(float v) {
  return gfx().textWidth(String((int)roundf(v))) + degreeSpace();
}

// Sets the first font from `fonts` (largest first) in which `text` fits within
// `maxW`, falling back to the last one. Returns the chosen font.
static const lgfx::IFont *fitFont(const String &text, int maxW,
                                  std::initializer_list<const lgfx::IFont *> fonts) {
  M5GFX &d = gfx();
  const lgfx::IFont *chosen = nullptr;
  for (const lgfx::IFont *f : fonts) {
    chosen = f;
    d.setFont(f);
    if (d.textWidth(text) <= maxW) break;
  }
  return chosen;
}

static int drawTempValue(float v, int x, int y, char align) {
  M5GFX &d = gfx();
  String num((int)roundf(v));
  int w = d.textWidth(num);
  int h = d.fontHeight();
  int r = h / 10 + 1;               // degree ring radius
  int degSpace = degreeSpace();

  int left, top;
  switch (align) {
    case 'R': left = x - w - degSpace; top = y - h / 2; break;
    case 'C': left = x - (w + degSpace) / 2; top = y; break;
    default:  left = x; top = y - h / 2; break;  // 'L'
  }

  auto prevDatum = d.getTextDatum();
  d.setTextDatum(top_left);
  d.drawString(num, left, top);
  d.setTextDatum(prevDatum);

  int cx = left + w + r + 2;
  int cy = top + (int)(h * 0.18f);
  d.drawCircle(cx, cy, r, d.getTextStyle().fore_rgb888);
  d.drawCircle(cx, cy, r - 1, d.getTextStyle().fore_rgb888);
  return w + degSpace;
}


// ---------------------------------------------------------------------------
// Comic style primitives
// ---------------------------------------------------------------------------

// A filled panel with a heavy frame and an offset drop shadow.
static void drawComicPanel(int x, int y, int w, int h, const Theme *t, uint32_t fill) {
  M5GFX &d = gfx();
  d.fillRect(x + 4, y + 4, w, h, t->text);   // drop shadow
  d.fillRect(x, y, w, h, fill);
  for (int k = 0; k < 3; k++) d.drawRect(x + k, y + k, w - 2 * k, h - 2 * k, t->text);
}

// Speech bubble with a tail pointing at (tailX, tailY). The tail fill is
// drawn after the border so it opens a gap in the bubble's left edge.
static void drawSpeechBubble(int x, int y, int w, int h, int tailX, int tailY, const Theme *t) {
  M5GFX &d = gfx();
  d.fillRoundRect(x, y, w, h, 14, t->bg);
  d.drawRoundRect(x, y, w, h, 14, t->text);
  d.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 13, t->text);
  int ty1 = y + h / 2 - 8, ty2 = y + h / 2 + 8;
  d.fillTriangle(x + 6, ty1, x + 6, ty2, tailX, tailY, t->bg);
  d.drawLine(x + 6, ty1, tailX, tailY, t->text);
  d.drawLine(x + 7, ty1, tailX + 1, tailY, t->text);
  d.drawLine(x + 6, ty2, tailX, tailY, t->text);
  d.drawLine(x + 7, ty2, tailX + 1, tailY, t->text);
}

static const char *WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Convert a UTC epoch to local time at the configured location using the UTC
// offset Open-Meteo reported. Without a successful fetch the offset is unknown
// (it would silently be UTC), so this refuses until weather data is valid.
static bool localTime(time_t utc, struct tm &out) {
  if (!weather.valid || utc < 1600000000) return false;  // no offset / no NTP yet
  utc += weather.utcOffsetSeconds;
  gmtime_r(&utc, &out);
  return true;
}


// ---------------------------------------------------------------------------
// Time-circuit style (Back to the Future DeLorean dashboard)
// ---------------------------------------------------------------------------

// Segment bit order: A top, B tr, C br, D bottom, E bl, F tl, G middle.
static const uint8_t SEG_DIGITS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                       0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void drawSeg7Char(char c, int x, int y, int w, int h, int th, uint32_t color) {
  uint8_t m = 0;
  if (c >= '0' && c <= '9') m = SEG_DIGITS[c - '0'];
  else if (c == '-') m = 0x40;
  else return;  // space: leave the cell dark
  M5GFX &d = gfx();
  int hw = w - 2 * th;            // horizontal segment length
  int vh = (h - 3 * th) / 2;      // vertical segment length
  if (m & 0x01) d.fillRect(x + th, y, hw, th, color);
  if (m & 0x02) d.fillRect(x + w - th, y + th, th, vh, color);
  if (m & 0x04) d.fillRect(x + w - th, y + 2 * th + vh, th, vh, color);
  if (m & 0x08) d.fillRect(x + th, y + h - th, hw, th, color);
  if (m & 0x10) d.fillRect(x, y + 2 * th + vh, th, vh, color);
  if (m & 0x20) d.fillRect(x, y + th, th, vh, color);
  if (m & 0x40) d.fillRect(x + th, y + th + vh, hw, th, color);
}

// Draw `sv` right-aligned inside a group whose right edge is `xRight`.
static void drawSeg7Right(const String &sv, int xRight, int y, int w, int h,
                          int th, int gap, uint32_t color) {
  int x = xRight - (int)sv.length() * (w + gap) + gap;
  for (size_t i = 0; i < sv.length(); i++) {
    drawSeg7Char(sv[i], x, y, w, h, th, color);
    x += w + gap;
  }
}

static const char *MONTHS_UP[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// One circuit row: month/day/year plus two 2-digit fields (hour:min, or
// hi/lo temps). ampm: 0 = AM, 1 = PM, -1 = hide the lamps.
static void drawCircuitRow(int y, const char *title, uint32_t color,
                           const struct tm *date, const String &f1, const String &f2,
                           const char *l1, const char *l2, int ampm, bool colon,
                           const Theme *t) {
  M5GFX &d = gfx();
  const int DW = 26, DH = 38, TH = 5, GAP = 6;
  const int digY = y + 14;

  d.drawRect(10, y, 580, 74, t->text);

  // Tiny labels above each group.
  d.setFont(&fonts::Font0);
  d.setTextDatum(top_center);
  d.setTextColor(t->text, t->bg);
  d.drawString("MONTH", 76, y + 4);
  d.drawString("DAY", 169, y + 4);
  d.drawString("YEAR", 262, y + 4);
  d.drawString(l1, 429, y + 4);
  d.drawString(l2, 505, y + 4);

  if (date) {
    d.setTextDatum(middle_center);
    d.setTextColor(color, t->bg);
    d.drawString(MONTHS_UP[date->tm_mon], 76, digY + DH / 2, &fonts::FreeSansBold18pt7b);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", date->tm_mday);
    drawSeg7Right(buf, 198, digY, DW, DH, TH, GAP, color);
    snprintf(buf, sizeof(buf), "%d", date->tm_year + 1900);
    drawSeg7Right(buf, 326, digY, DW, DH, TH, GAP, color);
  } else {
    drawSeg7Right("--", 198, digY, DW, DH, TH, GAP, color);
    drawSeg7Right("----", 326, digY, DW, DH, TH, GAP, color);
  }

  // AM/PM lamps.
  if (ampm >= 0) {
    d.setFont(&fonts::Font0);
    d.setTextDatum(middle_left);
    d.setTextColor(t->text, t->bg);
    if (ampm == 0) d.fillCircle(352, digY + 10, 4, color); else d.drawCircle(352, digY + 10, 4, t->text);
    if (ampm == 1) d.fillCircle(352, digY + 28, 4, color); else d.drawCircle(352, digY + 28, 4, t->text);
    d.drawString("AM", 361, digY + 10);
    d.drawString("PM", 361, digY + 28);
  }

  drawSeg7Right(f1, 458, digY, DW, DH, TH, GAP, color);
  if (colon) {
    d.fillRect(465, digY + 10, 5, 5, color);
    d.fillRect(465, digY + 24, 5, 5, color);
  }
  drawSeg7Right(f2, 534, digY, DW, DH, TH, GAP, color);

  // Title plate under the digits.
  d.fillRect(140, y + 58, 320, 14, t->text);
  d.setFont(&fonts::Font0);
  d.setTextDatum(middle_center);
  d.setTextColor(t->bg, t->text);
  d.drawString(title, 300, y + 65);
  d.setTextColor(t->text, t->bg);
}

static void renderTimeCircuit(const Theme *t) {
  M5GFX &d = gfx();
  const uint32_t RED = t->tempHi, GREEN = t->accent, YELLOW = t->sun;
  const bool imperial = config.units == "imperial";

  d.startWrite();
  d.fillScreen(t->bg);

  struct tm now, dest, dep;
  bool haveNow = localTime(time(nullptr), now);
  bool haveDest = localTime(time(nullptr) + 86400, dest);
  bool haveDep = localTime(weather.fetchedAt, dep);
  char f1[8], f2[8];

  // Row 1: tomorrow's forecast rides in the destination slot.
  String hi = "--", lo = "--";
  if (weather.valid) {
    hi = String((int)roundf(weather.daily[1].tempMax));
    lo = String((int)roundf(weather.daily[1].tempMin));
  }
  drawCircuitRow(6, "DESTINATION TIME", RED, haveDest ? &dest : nullptr,
                 hi, lo, "HI", "LO", -1, false, t);

  if (haveNow) {
    int h12 = now.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(f1, sizeof(f1), "%02d", h12);
    snprintf(f2, sizeof(f2), "%02d", now.tm_min);
    drawCircuitRow(88, "PRESENT TIME", GREEN, &now, f1, f2, "HOUR", "MIN",
                   now.tm_hour < 12 ? 0 : 1, true, t);
  } else {
    drawCircuitRow(88, "PRESENT TIME", GREEN, nullptr, "--", "--", "HOUR", "MIN", -1, true, t);
  }

  if (haveDep) {
    int h12 = dep.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(f1, sizeof(f1), "%02d", h12);
    snprintf(f2, sizeof(f2), "%02d", dep.tm_min);
    drawCircuitRow(170, "LAST TIME DEPARTED", YELLOW, &dep, f1, f2, "HOUR", "MIN",
                   dep.tm_hour < 12 ? 0 : 1, true, t);
  } else {
    drawCircuitRow(170, "LAST TIME DEPARTED", YELLOW, nullptr, "--", "--", "HOUR", "MIN", -1, true, t);
  }

  // Bottom strip: outdoor temp, humidity/wind, indoor SHT40, condition.
  d.drawRect(10, 252, 580, 142, t->text);
  d.setFont(&fonts::Font0);
  d.setTextDatum(top_left);
  d.setTextColor(t->text, t->bg);
  d.drawString(imperial ? "OUTDOOR TEMP F" : "OUTDOOR TEMP C", 30, 262);

  if (weather.valid) {
    String ot = String((int)roundf(weather.temperature));
    int xr = 30 + 3 * 48;  // fixed 3-cell field
    drawSeg7Right(ot, xr, 276, 40, 62, 8, 8, YELLOW);
  }

  d.setFont(&fonts::FreeSans12pt7b);
  d.setTextDatum(top_left);
  d.setTextColor(GREEN, t->bg);
  if (weather.valid) {
    d.drawString("HUM " + String(weather.humidity) + "%", 230, 280);
    d.drawString("WIND " + String((int)roundf(weather.windSpeed)) +
                 (imperial ? " MPH" : " KMH"), 230, 312);
  }

  d.setFont(&fonts::Font0);
  d.setTextColor(t->text, t->bg);
  d.drawString(imperial ? "INDOOR TEMP F / RH" : "INDOOR TEMP C / RH", 410, 262);
  if (room.valid) {
    float rt = imperial ? room.tempC * 9.0f / 5.0f + 32.0f : room.tempC;
    drawSeg7Right(String((int)roundf(rt)), 470, 276, 24, 36, 5, 5, YELLOW);
    drawSeg7Right(String((int)roundf(room.humidity)), 545, 276, 24, 36, 5, 5, YELLOW);
    d.setFont(&fonts::FreeSans9pt7b);
    d.setTextColor(YELLOW, t->bg);
    d.drawString("%", 550, 292);
  }

  d.setTextDatum(top_center);
  d.setTextColor(t->text, t->bg);
  if (weather.valid) {
    String cond = wmoDescription(weather.code);
    cond.toUpperCase();
    d.drawString(cond, 300, 352, &fonts::FreeSansBold12pt7b);
  } else {
    d.drawString("WAITING FOR WEATHER DATA", 300, 352, &fonts::FreeSansBold12pt7b);
  }
  d.setFont(&fonts::Font0);
  String place = config.placeName; place.toUpperCase();
  d.drawString(place, 300, 380);

  d.setTextDatum(top_left);
  d.endWrite();
  d.display();
}

// ---------------------------------------------------------------------------
// DECK//OS style: the Tab5 cyberdeck's HUD, translated to six inks
// ---------------------------------------------------------------------------
//
// Layout mirrors the deck's weather module: header with module title and
// status chips, a chamfered left panel (location, hero temperature, condition,
// a "// NOW" readout), a trace chart, and a row of forecast tiles with the
// current day lit in the accent. The deck's dim grays become one ink
// (t->subtext) with thinner mono faces for hierarchy; its faint background
// grid becomes dotted lines so it stays quiet without dithering. Fonts:
// Orbitron (the deck's display face, bundled with M5GFX) for the title and
// hero number, FreeMono for HUD text, Font2 for tiny readouts.

// Chamfer corner flags, same bits as the deck's deck_panel().
enum : uint8_t { CUT_TL = 1, CUT_TR = 2, CUT_BR = 4, CUT_BL = 8 };

static const int GRID_STEP = 30;   // grid cell size
static const int GRID_DOT = 3;     // one ink dot every N px along a grid line

static void drawDeckGrid(const Theme *t) {
  M5GFX &d = gfx();
  for (int x = GRID_STEP; x < W; x += GRID_STEP)
    for (int y = 0; y < H; y += GRID_DOT) d.drawPixel(x, y, t->grid);
  for (int y = GRID_STEP; y < H; y += GRID_STEP)
    for (int x = 0; x < W; x += GRID_DOT) d.drawPixel(x, y, t->grid);
}

// Single-pixel outline of a rectangle with the flagged corners cut at 45
// degrees by `c` px.
static void strokeChamfer(int x, int y, int w, int h, uint8_t cuts, int c, uint32_t col) {
  M5GFX &d = gfx();
  int x2 = x + w - 1, y2 = y + h - 1;
  int px[8], py[8], n = 0;
  auto add = [&](int ax, int ay) { px[n] = ax; py[n] = ay; n++; };
  if (cuts & CUT_TL) { add(x, y + c); add(x + c, y); } else add(x, y);
  if (cuts & CUT_TR) { add(x2 - c, y); add(x2, y + c); } else add(x2, y);
  if (cuts & CUT_BR) { add(x2, y2 - c); add(x2 - c, y2); } else add(x2, y2);
  if (cuts & CUT_BL) { add(x + c, y2); add(x, y2 - c); } else add(x, y2);
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    d.drawLine(px[i], py[i], px[j], py[j], col);
  }
}

// Opaque fill of the same chamfered shape; the cut-off corner triangles are
// left untouched so the background grid shows through them.
static void fillChamfer(int x, int y, int w, int h, uint8_t cuts, int c, uint32_t col) {
  M5GFX &d = gfx();
  int x2 = x + w - 1, y2 = y + h - 1;
  int tl = (cuts & CUT_TL) ? c : 0, tr = (cuts & CUT_TR) ? c : 0;
  int br = (cuts & CUT_BR) ? c : 0, bl = (cuts & CUT_BL) ? c : 0;
  d.fillRect(x, y + c, w, h - 2 * c, col);
  d.fillRect(x + tl, y, w - tl - tr, c, col);
  d.fillRect(x + bl, y + h - c, w - bl - br, c, col);
  if (tl) d.fillTriangle(x, y + c, x + c, y, x + c, y + c, col);
  if (tr) d.fillTriangle(x2 - c, y, x2, y + c, x2 - c, y + c, col);
  if (br) d.fillTriangle(x2, y2 - c, x2 - c, y2, x2 - c, y2 - c, col);
  if (bl) d.fillTriangle(x, y2 - c, x + c, y2, x + c, y2 - c, col);
}

// Panel: filled in the background color, 2 px outline, optional accent tab
// along the top edge (the deck's "lit" marker). The inner stroke uses a cut
// one px smaller so the two 45-degree passes sit flush with no gaps.
static void drawDeckPanel(int x, int y, int w, int h, int c, uint8_t cuts,
                          uint32_t outline, uint32_t tab, int tabW, const Theme *t) {
  fillChamfer(x, y, w, h, cuts, c, t->bg);
  strokeChamfer(x, y, w, h, cuts, c, outline);
  strokeChamfer(x + 1, y + 1, w - 2, h - 2, cuts, c - 1, outline);
  if (tabW > 0) {
    int tx = x + ((cuts & CUT_TL) ? c : 0) + 6;
    gfx().fillRect(tx, y, tabW, 3, tab);
  }
}

// A 2 px line, thickened perpendicular to its dominant axis.
static void drawLine2(int x1, int y1, int x2, int y2, uint32_t col) {
  M5GFX &d = gfx();
  d.drawLine(x1, y1, x2, y2, col);
  if (abs(y2 - y1) > abs(x2 - x1)) d.drawLine(x1 + 1, y1, x2 + 1, y2, col);
  else d.drawLine(x1, y1 + 1, x2, y2 + 1, col);
}

// "// TITLE" in the accent followed by a rule out to xRight (deck_section).
static void drawDeckSection(const char *title, int x, int y, int xRight, const Theme *t) {
  M5GFX &d = gfx();
  d.setFont(&fonts::FreeMonoBold9pt7b);
  d.setTextDatum(middle_left);
  d.setTextColor(t->accent, t->bg);
  String s = String("// ") + title;
  d.drawString(s, x, y);
  int rx = x + d.textWidth(s) + 8;
  if (rx < xRight) d.fillRect(rx, y, xRight - rx, 2, t->subtext);
}

// Pick the first font the string fits in at `maxW`; falls back to `small`.
static const lgfx::IFont *fitFont(const String &s, int maxW,
                                  const lgfx::IFont *big, const lgfx::IFont *small) {
  gfx().setFont(big);
  if (gfx().textWidth(s) <= maxW) return big;
  gfx().setFont(small);
  return small;
}

// Hi / lo / rain-chance polylines across the 5 forecast days inside the plot
// area (x, y, w, h). Temperature scale spans the week's lows to highs with a
// minimum 10-degree span so a flat week does not get amplified into noise.
static void drawDeckTrace(int x, int y, int w, int h, const Theme *t) {
  M5GFX &d = gfx();
  float tmin = 1e9f, tmax = -1e9f;
  for (int i = 0; i < FORECAST_DAYS; i++) {
    tmin = fminf(tmin, weather.daily[i].tempMin);
    tmax = fmaxf(tmax, weather.daily[i].tempMax);
  }
  if (tmax - tmin < 10.0f) {
    float mid = (tmax + tmin) * 0.5f;
    tmin = mid - 5.0f; tmax = mid + 5.0f;
  }
  tmin -= 2.0f; tmax += 2.0f;

  auto px = [&](int i) { return x + (int)((i + 0.5f) * w / FORECAST_DAYS); };
  auto py = [&](float v) { return y + h - 1 - (int)((v - tmin) / (tmax - tmin) * (h - 1)); };
  auto pr = [&](int p) { return y + h - 1 - (int)(p / 100.0f * (h - 1)); };

  // Dotted guides: one per day column, three horizontal quarters.
  for (int i = 0; i < FORECAST_DAYS; i++)
    for (int yy = y; yy < y + h; yy += GRID_DOT) d.drawPixel(px(i), yy, t->grid);
  for (int q = 1; q <= 3; q++) {
    int yy = y + h * q / 4;
    for (int xx = x; xx < x + w; xx += GRID_DOT) d.drawPixel(xx, yy, t->grid);
  }

  for (int i = 0; i + 1 < FORECAST_DAYS; i++) {
    const DailyForecast &a = weather.daily[i], &b = weather.daily[i + 1];
    drawLine2(px(i), pr(a.precipProb), px(i + 1), pr(b.precipProb), t->accent2);
    d.drawLine(px(i), py(a.tempMin), px(i + 1), py(b.tempMin), t->subtext);
    drawLine2(px(i), py(a.tempMax), px(i + 1), py(b.tempMax), t->accent);
  }
  for (int i = 0; i < FORECAST_DAYS; i++) {
    const DailyForecast &dd = weather.daily[i];
    d.fillCircle(px(i), pr(dd.precipProb), 2, t->accent2);
    d.fillCircle(px(i), py(dd.tempMin), 2, t->subtext);
    d.fillCircle(px(i), py(dd.tempMax), 3, t->accent);
  }
  d.fillRect(x, y + h, w, 2, t->accent2);   // the deck's baseline bar
}

static void drawDeckHeader(const Theme *t) {
  M5GFX &d = gfx();

  // "M5 // WEATHER": module tag in the accent, slashes dim, name in text.
  d.setFont(&fonts::Orbitron_Light_24);
  d.setTextDatum(top_left);
  int x = 16;
  d.setTextColor(t->accent, t->bg);
  d.drawString("M5", x, 5);       x += d.textWidth("M5 ");
  d.setTextColor(t->subtext, t->bg);
  d.drawString("//", x, 5);       x += d.textWidth("// ");
  d.setTextColor(t->text, t->bg);
  d.drawString("WEATHER", x, 5);

  // Status chips, right to left: battery, network, date.
  d.setFont(&fonts::Font2);
  d.setTextDatum(middle_right);
  int rx = W - 16;
  int batt = M5.Power.getBatteryLevel();
  if (batt >= 0) {
    d.setTextColor(t->accent, t->bg);
    String b = "BAT " + String(batt) + "%";
    d.drawString(b, rx, 17);
    rx -= d.textWidth(b) + 14;
  }
  if (WiFi.status() == WL_CONNECTED) {
    d.setTextColor(t->accent, t->bg);
    d.drawString("NET", rx, 17);
    rx -= d.textWidth("NET") + 14;
  }
  struct tm lt;
  if (localTime(time(nullptr), lt)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %02d %s %d", WEEKDAYS[lt.tm_wday], lt.tm_mday,
             MONTHS_UP[lt.tm_mon], lt.tm_year + 1900);
    String ds(buf); ds.toUpperCase();
    d.setTextColor(t->subtext, t->bg);
    d.drawString(ds, rx, 17);
  }

  // Header rule with the deck's accent bar under the title and a short
  // secondary bar beside it.
  d.drawFastHLine(0, 34, W, t->subtext);
  d.fillRect(16, 33, 80, 3, t->accent);
  d.fillRect(120, 33, 32, 3, t->accent2);
}

// Label in the dim mono face at the left edge of a readout row.
static void drawDeckLabel(const char *label, int x, int y, const Theme *t) {
  M5GFX &d = gfx();
  d.setFont(&fonts::FreeMono9pt7b);
  d.setTextDatum(middle_left);
  d.setTextColor(t->subtext, t->bg);
  d.drawString(label, x, y);
  d.setFont(&fonts::FreeMonoBold9pt7b);
  d.setTextDatum(middle_right);
  d.setTextColor(t->text, t->bg);
}

static void renderDeck(const Theme *t) {
  M5GFX &d = gfx();
  const bool imperial = config.units == "imperial";

  // Icons follow the deck's monochrome outline style: accent strokes, hollow
  // clouds, secondary neon for rain.
  Theme icon = *t;
  icon.text = t->accent;
  icon.subtext = t->accent;
  icon.sun = t->accent;
  icon.cloud = t->bg;
  icon.rain = t->accent2;

  // Geometry (600 x 400).
  const int LX = 10, LY = 44, LW = 220, LH = 346;      // left panel
  const int LC = 24, LR = LX + LW - 14;                // content left / right
  const int RX = 242, RW = 348;                        // right column
  const int CY = 62, CH = 160, CC = 10;                // chart panel
  const int PX = RX + 16, PY = CY + 14, PW = RW - 32, PH = CH - 32;  // plot area
  const int TY = 234, TH = 156, TW = 66, TGAP = 4, TX0 = 243;        // tiles

  d.startWrite();
  d.fillScreen(t->bg);
  drawDeckGrid(t);
  drawDeckHeader(t);

  drawDeckPanel(LX, LY, LW, LH, 14, CUT_TL | CUT_BR, t->subtext, t->accent2, 40, t);
  drawDeckSection("5D TRACE", RX, 50, RX + 150, t);
  drawDeckPanel(RX, CY, RW, CH, CC, CUT_TL | CUT_BR, t->subtext, 0, 0, t);

  // Chart legend, right-aligned on the section row.
  d.setFont(&fonts::Font2);
  d.setTextDatum(middle_right);
  int lx = W - 16;
  d.setTextColor(t->accent2, t->bg); d.drawString("RAIN %", lx, 50); lx -= d.textWidth("RAIN %") + 10;
  d.setTextColor(t->subtext, t->bg); d.drawString("LO", lx, 50);     lx -= d.textWidth("LO") + 10;
  d.setTextColor(t->accent, t->bg);  d.drawString("HI", lx, 50);

  if (!weather.valid) {
    for (int i = 0; i < FORECAST_DAYS; i++)
      drawDeckPanel(TX0 + i * (TW + TGAP), TY, TW, TH, 8, CUT_TL | CUT_BR, t->subtext, 0, 0, t);
    d.setFont(&fonts::FreeMonoBold12pt7b);
    d.setTextDatum(middle_center);
    d.setTextColor(t->accent2, t->bg);
    d.drawString("UPLINK // OPEN-METEO...", RX + RW / 2, CY + CH / 2);
    d.setFont(&fonts::FreeMono9pt7b);
    d.setTextColor(t->subtext, t->bg);
    d.drawString("NO DATA", LX + LW / 2, LY + LH / 2);
    d.setTextDatum(top_left);
    d.endWrite();
    d.display();
    return;
  }

  // --- Left panel: location pin + name ---
  d.fillCircle(29, 64, 4, t->text);
  d.fillTriangle(25, 66, 33, 66, 29, 74, t->text);
  d.fillCircle(29, 64, 1, t->bg);
  String place = config.placeName.length() ? config.placeName : "M5WEATHER";
  d.setFont(fitFont(place, LR - 40, &fonts::FreeMonoBold12pt7b, &fonts::FreeMonoBold9pt7b));
  d.setTextDatum(middle_left);
  d.setTextColor(t->text, t->bg);
  d.drawString(place, 40, 66);

  // Hero: icon left, Orbitron temperature right.
  drawWeatherIcon(weather.code, weather.isDay, 46, 118, 26, &icon);
  d.setFont(&fonts::Orbitron_Light_24);
  d.setTextSize(2);
  d.setTextColor(t->text, t->bg);
  drawTempValue(weather.temperature, 84, 118, 'L');
  d.setTextSize(1);

  // Condition in the secondary neon, then today's H / L in dim.
  String cond = wmoDescription(weather.code);
  cond.toUpperCase();
  d.setFont(fitFont(cond, LR - LC, &fonts::FreeMonoBold12pt7b, &fonts::FreeMonoBold9pt7b));
  d.setTextDatum(top_left);
  d.setTextColor(t->accent2, t->bg);
  d.drawString(cond, LC, 152);

  d.setFont(&fonts::FreeMono9pt7b);
  d.setTextDatum(middle_left);
  d.setTextColor(t->subtext, t->bg);
  int hx = LC;
  d.drawString("H", hx, 192);  hx += d.textWidth("H ");
  hx += drawTempValue(weather.daily[0].tempMax, hx, 192, 'L') + d.textWidth("  ");
  d.drawString("L", hx, 192);  hx += d.textWidth("L ");
  drawTempValue(weather.daily[0].tempMin, hx, 192, 'L');

  // "// NOW" readout rows: label dim-left, value bold-right.
  drawDeckSection("NOW", LC, 214, LR, t);
  int ry = 238;
  const int ROW = 26;
  drawDeckLabel("FEELS", LC, ry, t);
  drawTempValue(weather.feelsLike, LR, ry, 'R');
  ry += ROW;
  drawDeckLabel("HUMIDITY", LC, ry, t);
  d.drawString(String(weather.humidity) + "%", LR, ry);
  ry += ROW;
  drawDeckLabel("WIND", LC, ry, t);
  d.drawString(String((int)roundf(weather.windSpeed)) + (imperial ? " MPH" : " KMH"), LR, ry);
  ry += ROW;
  drawDeckLabel("RAIN", LC, ry, t);
  d.drawString(String(weather.daily[0].precipProb) + "%", LR, ry);
  ry += ROW;
  if (room.valid) {
    drawDeckLabel("INDOOR", LC, ry, t);
    String rh = String((int)roundf(room.humidity)) + "%";
    d.drawString(rh, LR, ry);
    float roomT = imperial ? room.tempC * 9.0f / 5.0f + 32.0f : room.tempC;
    drawTempValue(roomT, LR - d.textWidth(rh) - 10, ry, 'R');
  }

  // Footer: fetch time, deck phrasing.
  struct tm ft;
  if (localTime(weather.fetchedAt, ft)) {
    int h12 = ft.tm_hour % 12; if (h12 == 0) h12 = 12;
    char buf[32];
    snprintf(buf, sizeof(buf), "LIVE // UPDATED %d:%02d %s", h12, ft.tm_min,
             ft.tm_hour < 12 ? "AM" : "PM");
    d.setFont(&fonts::Font2);
    d.setTextDatum(middle_left);
    d.setTextColor(t->subtext, t->bg);
    d.drawString(buf, LC, 372);
  }

  // --- Trace chart ---
  drawDeckTrace(PX, PY, PW, PH, t);

  // --- Forecast tiles ---
  for (int i = 0; i < FORECAST_DAYS; i++) {
    const DailyForecast &day = weather.daily[i];
    const bool lit = (i == 0);
    int tx = TX0 + i * (TW + TGAP);
    int cx = tx + TW / 2;
    drawDeckPanel(tx, TY, TW, TH, 8, CUT_TL | CUT_BR,
                  lit ? t->accent : t->subtext, t->accent, lit ? 24 : 0, t);

    String name = lit ? "TODAY" : String(WEEKDAYS[day.weekday]);
    name.toUpperCase();
    d.setFont(&fonts::FreeSansBold9pt7b);
    d.setTextDatum(top_center);
    d.setTextColor(lit ? t->accent : t->text, t->bg);
    d.drawString(name, cx, TY + 14);

    drawWeatherIcon(day.code, true, cx, TY + 56, 16, &icon);

    d.setFont(&fonts::FreeMonoBold12pt7b);
    d.setTextDatum(top_center);
    d.setTextColor(t->text, t->bg);
    drawTempValue(day.tempMax, cx, TY + 84, 'C');

    d.setFont(&fonts::FreeMono9pt7b);
    d.setTextColor(t->subtext, t->bg);
    drawTempValue(day.tempMin, cx, TY + 110, 'C');

    d.setFont(&fonts::Font2);
    d.setTextColor(day.precipProb >= 20 ? t->accent2 : t->subtext, t->bg);
    d.drawString(String(day.precipProb) + "%", cx, TY + 132);
  }

  d.setTextDatum(top_left);
  d.endWrite();
  d.display();
}

void renderWeather() {
  const Theme *t = activeTheme();
  if (t->style == STYLE_CIRCUIT) { renderTimeCircuit(t); return; }
  if (t->style == STYLE_DECK) { renderDeck(t); return; }
  M5GFX &d = gfx();

  d.startWrite();
  d.fillScreen(t->bg);
  d.setTextColor(t->text, t->bg);

  // --- Header: place, date, updated time ---
  d.setFont(&fonts::FreeSansBold12pt7b);
  String place = config.placeName.length() ? config.placeName : "M5Weather";
  if (t->style == STYLE_COMIC) {
    // Yellow caption box, comic-cover style, behind the place name.
    drawComicPanel(12, 8, d.textWidth(place) + 24, 40, t, t->sun);
    d.setTextDatum(middle_left);
    d.drawString(place, 24, 29);
  } else {
    d.setTextDatum(top_left);
    d.setCursor(16, 14);
    d.print(place);
  }

  // Date is "now"; the updated stamp is when the weather was actually fetched.
  struct tm lt, ft;
  if (localTime(time(nullptr), lt) && localTime(weather.fetchedAt, ft)) {
    char dateBuf[40], timeBuf[24];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
             WEEKDAYS[lt.tm_wday], MONTHS[lt.tm_mon], lt.tm_mday);
    int h12 = ft.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", h12, ft.tm_min,
             ft.tm_hour < 12 ? "AM" : "PM");
    d.setTextDatum(top_right);
    d.drawString(dateBuf, W - 16, 14, &fonts::FreeSans12pt7b);
    d.setTextColor(t->subtext, t->bg);
    d.drawString(String("updated ") + timeBuf, W - 16, 42, &fonts::FreeSans9pt7b);
    d.setTextColor(t->text, t->bg);
  }
  if (t->style != STYLE_COMIC) d.fillRect(16, 64, W - 32, 3, t->accent);

  if (!weather.valid) {
    d.setTextDatum(middle_center);
    d.drawString("Waiting for weather data...", W / 2, H / 2, &fonts::FreeSans18pt7b);
    d.setTextDatum(top_left);
    d.endWrite();
    d.display();
    return;
  }

  // --- Current conditions ---
  const int curY = 155;
  const bool imperial = config.units == "imperial";
  const float roomT = imperial ? room.tempC * 9.0f / 5.0f + 32.0f : room.tempC;
  const String humStr = String(weather.humidity) + "%";
  const String windStr = String((int)roundf(weather.windSpeed)) + (imperial ? " mph" : " km/h");
  const String rhStr = String((int)roundf(room.humidity)) + "%";

  // Right column geometry: labels left-aligned at rx, values right-aligned at
  // the edge. rx moves left from 445 as far as the widest row needs (the
  // indoor row carries two values), so a label never runs into its value.
  const int RIGHT = W - 16, LABEL_GAP = 10, PAIR_GAP = 8, rowGap = 38;
  d.setFont(&fonts::FreeSans12pt7b);
  int need = d.textWidth("Feels like") + LABEL_GAP + tempValueWidth(weather.feelsLike);
  need = std::max(need, (int)d.textWidth("Humidity") + LABEL_GAP + (int)d.textWidth(humStr));
  need = std::max(need, (int)d.textWidth("Wind") + LABEL_GAP + (int)d.textWidth(windStr));
  if (room.valid)
    need = std::max(need, (int)d.textWidth("Indoor") + LABEL_GAP + tempValueWidth(roomT) +
                              PAIR_GAP + (int)d.textWidth(rhStr));
  const int rx = std::min(445, RIGHT - need);
  const int ry = curY - 60;
  // The condition text shares a line with the indoor row, so it must end
  // before rx when that row is shown; otherwise it has the full width.
  const int descRight = (room.valid ? rx : RIGHT) - 12;

  if (t->style == STYLE_COMIC) drawComicPanel(10, 72, 580, 168, t, t->bg);
  drawWeatherIcon(weather.code, weather.isDay, 110, curY, 68, t);

  d.setTextDatum(middle_left);
  d.setFont(&fonts::FreeSansBold24pt7b);
  d.setTextSize(2);
  drawTempValue(weather.temperature, 205, curY - 10, 'L');
  d.setTextSize(1);

  String desc = wmoDescription(weather.code);
  if (t->style == STYLE_COMIC) {
    // Condition text in a speech bubble pointing at the weather icon.
    int bx = 212, by = curY + 40, bh = 40;
    fitFont(desc, descRight - bx - 30,
            {&fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b, &fonts::FreeSans9pt7b});
    int bw = d.textWidth(desc) + 30;
    drawSpeechBubble(bx, by, bw, bh, 172, curY + 74, t);
    d.drawString(desc, bx + 18, by + bh / 2);
  } else {
    const int dx = 208;
    fitFont(desc, descRight - dx,
            {&fonts::FreeSans18pt7b, &fonts::FreeSans12pt7b, &fonts::FreeSans9pt7b});
    d.drawString(desc, dx, curY + 52);
  }

  // Right column: feels like / humidity / wind / indoor (from the SHT40)
  d.setFont(&fonts::FreeSans12pt7b);
  d.setTextColor(t->subtext, t->bg);
  d.drawString("Feels like", rx, ry);
  d.drawString("Humidity", rx, ry + rowGap);
  d.drawString("Wind", rx, ry + rowGap * 2);
  if (room.valid) d.drawString("Indoor", rx, ry + rowGap * 3);
  d.setTextColor(t->text, t->bg);
  d.setTextDatum(middle_right);
  drawTempValue(weather.feelsLike, RIGHT, ry, 'R');
  d.drawString(humStr, RIGHT, ry + rowGap);
  d.drawString(windStr, RIGHT, ry + rowGap * 2);
  if (room.valid) {
    // "72°  45%": humidity right-aligned, temperature to its left.
    d.drawString(rhStr, RIGHT, ry + rowGap * 3);
    drawTempValue(roomT, RIGHT - d.textWidth(rhStr) - PAIR_GAP, ry + rowGap * 3, 'R');
  }

  // --- 5-day forecast ---
  const int rowTop = 248;
  if (t->style != STYLE_COMIC) d.drawFastHLine(16, rowTop, W - 32, t->subtext);

  const int colW = (W - 32) / FORECAST_DAYS;
  for (int i = 0; i < FORECAST_DAYS; i++) {
    const DailyForecast &day = weather.daily[i];
    int cx = 16 + colW * i + colW / 2;
    if (t->style == STYLE_COMIC) drawComicPanel(16 + colW * i + 2, rowTop + 2, colW - 10, 144, t, t->bg);

    d.setTextDatum(top_center);
    d.setTextColor(i == 0 ? t->accent : t->text, t->bg);
    d.drawString(i == 0 ? "Today" : WEEKDAYS[day.weekday], cx, rowTop + 12,
                 &fonts::FreeSansBold9pt7b);
    d.setTextColor(t->text, t->bg);

    drawWeatherIcon(day.code, true, cx, rowTop + 68, 26, t);

    d.setFont(&fonts::FreeSansBold12pt7b);
    d.setTextDatum(top_center);
    d.setTextColor(t->tempHi, t->bg);
    drawTempValue(day.tempMax, cx - 20, rowTop + 102, 'C');
    d.setTextColor(t->tempLo, t->bg);
    drawTempValue(day.tempMin, cx + 24, rowTop + 102, 'C');
    d.setTextColor(t->text, t->bg);

    if (day.precipProb >= 20) {
      d.setTextColor(t->rain, t->bg);
      d.drawString(String(day.precipProb) + "%", cx, rowTop + (t->style == STYLE_COMIC ? 122 : 128),
                   &fonts::FreeSans9pt7b);
      d.setTextColor(t->text, t->bg);
    }

    if (t->style != STYLE_COMIC && i > 0) d.drawFastVLine(16 + colW * i, rowTop + 14, 118, t->subtext);
  }

  d.setTextDatum(top_left);
  d.endWrite();
  d.display();   // e-ink flush: takes several seconds with flashing, normal
}

void renderStatus(const String &title, const String &lines) {
  const Theme *t = themeById(config.theme);
  M5GFX &d = gfx();

  d.startWrite();
  d.fillScreen(t->bg);
  d.setTextColor(t->text, t->bg);
  d.setTextDatum(top_center);

  d.drawString("M5Weather", W / 2, 40, &fonts::FreeSansBold18pt7b);
  d.fillRect(W / 2 - 60, 88, 120, 3, t->accent);
  d.drawString(title, W / 2, 120, &fonts::FreeSans18pt7b);

  int y = 180;
  int start = 0;
  d.setFont(&fonts::FreeSans12pt7b);
  while (start < (int)lines.length()) {
    int nl = lines.indexOf('\n', start);
    if (nl < 0) nl = lines.length();
    d.drawString(lines.substring(start, nl), W / 2, y);
    y += 34;
    start = nl + 1;
  }

  d.setTextDatum(top_left);
  d.endWrite();
  d.display();
}
