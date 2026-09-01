#include "display.h"

#include <M5Unified.h>
#include <time.h>

#include "config.h"
#include "sensor.h"
#include "themes.h"
#include "weather.h"

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
// Uses the currently set font/size; returns nothing.
static void drawTempValue(float v, int x, int y, char align) {
  M5GFX &d = gfx();
  String num((int)roundf(v));
  int w = d.textWidth(num);
  int h = d.fontHeight();
  int r = h / 10 + 1;               // degree ring radius
  int degSpace = r * 2 + 3;

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

void renderWeather() {
  const Theme *t = themeById(config.theme);
  if (t->style == STYLE_CIRCUIT) { renderTimeCircuit(t); return; }
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
  if (t->style == STYLE_COMIC) drawComicPanel(10, 72, 580, 168, t, t->bg);
  drawWeatherIcon(weather.code, weather.isDay, 110, curY, 68, t);

  d.setTextDatum(middle_left);
  d.setFont(&fonts::FreeSansBold24pt7b);
  d.setTextSize(2);
  drawTempValue(weather.temperature, 215, curY - 10, 'L');
  d.setTextSize(1);

  if (t->style == STYLE_COMIC) {
    // Condition text in a speech bubble pointing at the weather icon.
    d.setFont(&fonts::FreeSansBold12pt7b);
    String desc = wmoDescription(weather.code);
    int bx = 212, by = curY + 40, bw = d.textWidth(desc) + 30, bh = 40;
    drawSpeechBubble(bx, by, bw, bh, 172, curY + 74, t);
    d.drawString(desc, bx + 18, by + bh / 2);
  } else {
    d.setFont(&fonts::FreeSans18pt7b);
    d.drawString(wmoDescription(weather.code), 218, curY + 52);
  }

  // Right column: feels like / humidity / wind / indoor (from the SHT40)
  const bool imperial = config.units == "imperial";
  const int rowGap = 38;
  d.setFont(&fonts::FreeSans12pt7b);
  d.setTextColor(t->subtext, t->bg);
  int rx = 445, ry = curY - 60;
  d.drawString("Feels like", rx, ry);
  d.drawString("Humidity", rx, ry + rowGap);
  d.drawString("Wind", rx, ry + rowGap * 2);
  if (room.valid) d.drawString("Indoor", rx, ry + rowGap * 3);
  d.setTextColor(t->text, t->bg);
  d.setTextDatum(middle_right);
  drawTempValue(weather.feelsLike, W - 16, ry, 'R');
  d.drawString(String(weather.humidity) + "%", W - 16, ry + rowGap);
  d.drawString(String((int)roundf(weather.windSpeed)) + (imperial ? " mph" : " km/h"),
               W - 16, ry + rowGap * 2);
  if (room.valid) {
    // "72°  45%": humidity right-aligned, temperature to its left.
    String rh = String((int)roundf(room.humidity)) + "%";
    d.drawString(rh, W - 16, ry + rowGap * 3);
    float roomT = imperial ? room.tempC * 9.0f / 5.0f + 32.0f : room.tempC;
    drawTempValue(roomT, W - 16 - d.textWidth(rh) - 12, ry + rowGap * 3, 'R');
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
