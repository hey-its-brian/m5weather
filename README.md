# M5Weather

A weather dashboard for the **M5Stack PaperColor** (ESP32-S3, 4" E Ink
Spectra 6 color display, 600x400) with a browser-based config UI on your
local network. Change the zip code, units, refresh interval, and theme from
your desktop without reflashing.

Weather data comes from [Open-Meteo](https://open-meteo.com/) and zip-code
geocoding from [Zippopotam.us](https://www.zippopotam.us/). Neither requires
an API key or account.

## Features

- Current conditions: temperature, feels-like, humidity, wind, condition icon
- Indoor temperature and humidity from the onboard SHT40 sensor
- 5-day forecast with highs (red), lows (blue), and precipitation chance
- Web config UI at **http://m5weather.local** (mDNS) or the device IP
- First-boot captive portal: the device opens a WPA2-protected
  `M5Weather-Setup` hotspot and walks you through Wi-Fi and zip setup
- Theme system (Classic / Night / Forest / Comic / Time Circuit built in)
  designed for adding more; a theme is a named palette plus an optional
  rendering style in `src/themes.cpp`
- Settings persist in flash (NVS) and survive power loss
- Hardware RTC is corrected from NTP, so reboots start with an accurate clock

## Requirements

- M5Stack PaperColor (the ESP32-S3 dev kit with the 4" Spectra 6 panel)
- USB-C cable
- [PlatformIO Core](https://platformio.org/install/cli) (or the PlatformIO
  VS Code extension)
- 2.4 GHz Wi-Fi network (the ESP32-S3 has no 5 GHz radio)

## Install

1. Clone and build. The first build downloads the ESP32 toolchain and
   libraries automatically; give it a few minutes.

   ```sh
   git clone https://github.com/hey-its-brian/m5weather.git
   cd m5weather
   pio run
   ```

2. Plug the PaperColor in over USB-C and flash:

   ```sh
   pio run -t upload
   ```

   If the upload cannot connect ("No serial data received"), put the device
   in download mode manually: hold BOOT, tap RST, release BOOT, then run the
   upload again.

3. First-time setup, on the device screen:
   - Join the Wi-Fi network **M5Weather-Setup** (password `m5weather`).
   - A config page opens automatically, or browse to `http://192.168.4.1`.
   - Enter your home Wi-Fi credentials. The device reboots and joins.
   - Browse to **http://m5weather.local**, enter your zip code, and click
     *Save & Update Display*.

4. Optional: watch logs with `pio device monitor` (115200 baud).

The device stays awake so the config page is always reachable; plan on USB
power. A full e-ink refresh takes several seconds with visible flashing,
which is normal for Spectra 6 panels.

## Config UI

| Setting | Notes |
|---|---|
| Zip code | US 5-digit; validated against Zippopotam.us before saving |
| Units | F/mph or C/km/h |
| Refresh | 15 min to 3 hours (default 30 min) |
| Theme | Palette applied to the e-ink render |
| Wi-Fi | Changing it reboots the device |

The page also shows battery %, Wi-Fi signal, and last-update time, plus a
*Refresh Weather Now* button.

## Adding a theme

Add an entry to `THEMES[]` in `src/themes.cpp` and it automatically appears
in the web UI dropdown. Stick to the six colors the Spectra 6 panel
physically has. The driver's exact palette values are defined at the top of
that file; anything else gets snapped to the nearest ink color because
dithering is disabled (dithering covers the panel in visible dot noise).

A theme can also set a rendering `style`. `STYLE_FLAT` (default) is the
plain dashboard; `STYLE_COMIC` reuses the flat layout with comic flourishes
(heavy panel frames with drop shadows, a yellow caption box, a speech
bubble); `STYLE_CIRCUIT` swaps in a fully custom renderer, the Back to the
Future time circuit: three seven-segment rows (red destination = tomorrow's
date with HI/LO temps, green present time, yellow last time departed = last
fetch) over a gauge strip with outdoor, indoor, humidity, and wind. To build
your own layout, add a style to the enum in `src/themes.h` and branch on it
in `src/display.cpp` (see `renderTimeCircuit`).

## Security model

Trusted-LAN, no login. Mitigations in place:

- **Host-header validation** on every route blocks DNS-rebinding attacks
  (a malicious website resolving its own domain to the device's IP).
- **Origin checks**: cross-site requests from web pages you visit are
  rejected with 403, so a drive-by page can't reconfigure the device.
- **Zip input** is validated (5 digits) server-side before being used in the
  geocoding URL.
- **Setup hotspot is WPA2-protected** (password shown on the e-ink screen),
  so credentials can't be injected by a stranger during first-time setup.
- **API responses are size-capped** (64KB, HTTP/1.0) so a spoofed server
  can't exhaust device memory.

Accepted risks: anyone on your LAN can change settings or reboot the device
(add a PIN if your network is shared); weather API TLS is unverified
(`setInsecure`) so a MITM could show you wrong weather; Wi-Fi credentials are
stored unencrypted in NVS flash (use ESP32 flash encryption if that matters
to you). There is no OTA endpoint, so firmware changes require USB.

## Version notes

### 1.2.0 (2026-09-01)

- Two new themes: Comic (heavy panel frames, drop shadows, caption box,
  speech bubble) and Time Circuit (Back to the Future DeLorean layout with
  seven-segment digits drawn from primitives)
- Theme system extended from palette-only to palette + rendering style,
  completing the "per-theme layout" roadmap item; the Spectra 6's red,
  green, and yellow inks happen to match the movie prop exactly
- Note: the Time Circuit theme trades the 5-day forecast for tomorrow's
  HI/LO in the destination row

### 1.1.0 (2026-09-01)

- Indoor temperature and humidity from the PaperColor's onboard SHT40,
  shown as an "Indoor" row under the current conditions and exposed as
  `room_temp_c` / `room_humidity` in `GET /api/status`
- M5Unified has no SHT4x class; `src/sensor.cpp` drives the chip directly
  over `M5.In_I2C` (address 0x44, command 0xFD, CRC-8 checked). It probes
  the internal bus first and falls back to the Grove port, so a Grove SHT40
  on another board works without changes
- The reading refreshes on every weather fetch, so the indoor row is at most
  one refresh interval old
- Fix: saving a new zip in the web UI now fetches weather immediately instead
  of leaving "Waiting for weather data" on screen until the next refresh timer
- Fix: the header's "updated" stamp now shows the time of the last successful
  fetch in local time; before any fetch it displayed UTC because the location's
  UTC offset was not known yet, so the header now stays hidden until then
- Flashing note: the PaperColor has no BOOT button. If esptool reports "No
  serial data received", hold the side power button for about 6 seconds with
  USB connected to enter download mode, flash, then press it once to reboot

### 1.0.0 (2026-08-27)

Initial release.

- Current conditions plus 5-day forecast rendered on the Spectra 6 panel
- Web config UI (zip, units, refresh interval, theme) at m5weather.local
- Captive-portal first-boot setup over a WPA2 hotspot
- Three built-in themes with an extensible palette registry
- Security hardening: Host/Origin request validation, server-side zip
  validation, size-capped API responses
- NTP time sync that distrusts the factory RTC value and writes the
  corrected time back to the RTC

Hardware findings baked into this release, for anyone porting:

- M5GFX 0.2.20+ autodetects the PaperColor at runtime (via its M5PM1 power
  IC); no board defines needed, but OPI PSRAM must be enabled or panel init
  fails
- The EPD dither modes (`epd_quality`, `epd_text`, `epd_fast`) Bayer-dither
  every pixel, including exact palette colors; use `epd_fastest` for flat UI
- The GFX fonts have no degree-sign glyph; this firmware draws the degree
  marker as a ring
- Open-Meteo responds with chunked transfer encoding, which HTTPClient's raw
  stream does not decode; read the body with `getString()` (or force
  HTTP/1.0) before JSON parsing
- Serial over the built-in USB-C port needs `ARDUINO_USB_MODE=1` and
  `ARDUINO_USB_CDC_ON_BOOT=1` build flags

## Roadmap

- [x] More themes; per-theme layout (not just palette): shipped in 1.2.0
- [ ] Hourly forecast view
- [ ] Optional deep-sleep battery mode
- [ ] Optional config-page PIN
- [ ] Non-US postal code support (Zippopotam supports ~60 countries)
