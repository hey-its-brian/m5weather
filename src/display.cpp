#include "display.h"

#include <M5Unified.h>
#include <time.h>

#include "config.h"
#include "themes.h"
#include "weather.h"

// Landscape: 600 wide x 400 tall after rotation.
static const int W = 600;
static const int H = 400;

static M5GFX &gfx() { return M5.Display; }

void displayInit() {
  gfx().setRotation(1);           // portrait panel -> landscape
  gfx().setEpdMode(epd_mode_t::epd_quality);
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

static String fmtTemp(float v) { return String((int)roundf(v)) + "\xC2\xB0"; }

static const char *WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Local time at the configured location, derived from NTP time (UTC) plus the
// UTC offset Open-Meteo reported for the location.
static bool localNow(struct tm &out) {
  time_t now = time(nullptr);
  if (now < 1600000000) return false;  // NTP not synced yet
  now += weather.utcOffsetSeconds;
  gmtime_r(&now, &out);
  return true;
}

void renderWeather() {
  const Theme *t = themeById(config.theme);
  M5GFX &d = gfx();

  d.startWrite();
  d.fillScreen(t->bg);
  d.setTextColor(t->text, t->bg);

  // --- Header: place, date, updated time ---
  d.setFont(&fonts::FreeSansBold12pt7b);
  d.setTextDatum(top_left);
  d.setCursor(16, 14);
  d.print(config.placeName.length() ? config.placeName : "M5Weather");

  struct tm lt;
  bool haveTime = localNow(lt);
  if (haveTime) {
    char dateBuf[40], timeBuf[24];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
             WEEKDAYS[lt.tm_wday], MONTHS[lt.tm_mon], lt.tm_mday);
    int h12 = lt.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", h12, lt.tm_min,
             lt.tm_hour < 12 ? "AM" : "PM");
    d.setTextDatum(top_right);
    d.drawString(dateBuf, W - 16, 14, &fonts::FreeSans12pt7b);
    d.setTextColor(t->subtext, t->bg);
    d.drawString(String("updated ") + timeBuf, W - 16, 42, &fonts::FreeSans9pt7b);
    d.setTextColor(t->text, t->bg);
  }
  d.fillRect(16, 64, W - 32, 3, t->accent);

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
  drawWeatherIcon(weather.code, weather.isDay, 110, curY, 68, t);

  d.setTextDatum(middle_left);
  d.setFont(&fonts::FreeSansBold24pt7b);
  d.setTextSize(2);
  d.drawString(fmtTemp(weather.temperature), 215, curY - 10);
  d.setTextSize(1);

  d.setFont(&fonts::FreeSans18pt7b);
  d.drawString(wmoDescription(weather.code), 218, curY + 52);

  // Right column: feels like / humidity / wind
  const bool imperial = config.units == "imperial";
  d.setFont(&fonts::FreeSans12pt7b);
  d.setTextColor(t->subtext, t->bg);
  int rx = 445, ry = curY - 52;
  d.drawString("Feels like", rx, ry);
  d.drawString("Humidity", rx, ry + 42);
  d.drawString("Wind", rx, ry + 84);
  d.setTextColor(t->text, t->bg);
  d.setTextDatum(middle_right);
  d.drawString(fmtTemp(weather.feelsLike), W - 16, ry);
  d.drawString(String(weather.humidity) + "%", W - 16, ry + 42);
  d.drawString(String((int)roundf(weather.windSpeed)) + (imperial ? " mph" : " km/h"),
               W - 16, ry + 84);

  // --- 5-day forecast ---
  const int rowTop = 248;
  d.drawFastHLine(16, rowTop, W - 32, t->subtext);

  const int colW = (W - 32) / FORECAST_DAYS;
  for (int i = 0; i < FORECAST_DAYS; i++) {
    const DailyForecast &day = weather.daily[i];
    int cx = 16 + colW * i + colW / 2;

    d.setTextDatum(top_center);
    d.setTextColor(i == 0 ? t->accent : t->text, t->bg);
    d.drawString(i == 0 ? "Today" : WEEKDAYS[day.weekday], cx, rowTop + 12,
                 &fonts::FreeSansBold9pt7b);
    d.setTextColor(t->text, t->bg);

    drawWeatherIcon(day.code, true, cx, rowTop + 68, 26, t);

    d.setFont(&fonts::FreeSansBold12pt7b);
    d.setTextDatum(top_center);
    d.setTextColor(t->tempHi, t->bg);
    d.drawString(fmtTemp(day.tempMax), cx - 20, rowTop + 102);
    d.setTextColor(t->tempLo, t->bg);
    d.drawString(fmtTemp(day.tempMin), cx + 24, rowTop + 102);
    d.setTextColor(t->text, t->bg);

    if (day.precipProb >= 20) {
      d.setTextColor(t->rain, t->bg);
      d.drawString(String(day.precipProb) + "%", cx, rowTop + 128, &fonts::FreeSans9pt7b);
      d.setTextColor(t->text, t->bg);
    }

    if (i > 0) d.drawFastVLine(16 + colW * i, rowTop + 14, 118, t->subtext);
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
