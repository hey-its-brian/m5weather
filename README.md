# M5Weather

A weather dashboard for the **M5Stack PaperColor** (ESP32-S3, 4" E Ink
Spectra 6 color display, 600×400) with a browser-based config UI on your
local network — change the zip code, units, refresh interval, and theme from
your desktop without reflashing.

Weather data comes from [Open-Meteo](https://open-meteo.com/) and zip-code
geocoding from [Zippopotam.us](https://www.zippopotam.us/). Neither requires
an API key or account.

## Features

- Current conditions: temperature, feels-like, humidity, wind, condition icon
- 5-day forecast with highs (red), lows (blue), and precipitation chance
- Web config UI at **http://m5weather.local** (mDNS) or the device IP
- First-boot captive portal: the device opens a `M5Weather-Setup` hotspot;
  join it and the config page pops up to collect Wi-Fi + zip
- Theme system (Classic / Night / Forest built in) designed for adding more —
  a theme is just a named palette in `src/themes.cpp`
- Settings persist in flash (NVS); survives power loss

## Building

Uses [PlatformIO](https://platformio.org/). From the repo root:

```sh
pio run                 # build
pio run -t upload       # flash over USB-C
pio device monitor      # serial logs (115200)
```

The `platformio.ini` pins the [pioarduino](https://github.com/pioarduino/platform-espressif32)
ESP32 platform (Arduino core 3.x) plus M5Unified/M5GFX ≥ 0.2.20, the first
M5GFX release with PaperColor support. No board-specific defines are needed —
M5GFX autodetects the PaperColor at runtime via its M5PM1 power IC.

## First-time setup

1. Flash the firmware. The display shows setup instructions.
2. On your phone or desktop, join the Wi-Fi network **M5Weather-Setup**.
3. A config page opens (or browse to `http://192.168.4.1`).
4. Enter your Wi-Fi credentials → the device reboots and joins your network.
5. Browse to **http://m5weather.local**, enter your zip code, hit
   *Save & Update Display*.

## Config UI

| Setting | Notes |
|---|---|
| Zip code | US 5-digit; validated against Zippopotam.us before saving |
| Units | °F/mph or °C/km/h |
| Refresh | 15 min – 3 hours (default 30 min) |
| Theme | Palette applied to the e-ink render |
| Wi-Fi | Changing it reboots the device |

The page also shows battery %, Wi-Fi signal, and last-update time, plus a
*Refresh Weather Now* button.

## Adding a theme

Add an entry to `THEMES[]` in `src/themes.cpp` — it automatically appears in
the web UI dropdown. Stick to the six colors the Spectra 6 panel physically
has (black, white, red, yellow, blue, green); M5GFX dithers everything else.

## Security model

Trusted-LAN, no login. Mitigations in place:

- **Host-header validation** on every route — blocks DNS-rebinding attacks
  (a malicious website resolving its own domain to the device's IP).
- **Origin checks** — cross-site requests from web pages you visit are
  rejected with 403, so a drive-by page can't reconfigure the device.
- **Zip input** is validated (5 digits) server-side before being used in the
  geocoding URL.
- **Setup hotspot is WPA2-protected** (password shown on the e-ink screen),
  so credentials can't be injected by a stranger during first-time setup.
- **API responses are size-capped** (64KB, HTTP/1.0) so a spoofed server
  can't exhaust device memory.

Accepted risks: anyone on your LAN can change settings or reboot the device
(add a PIN if your network is shared); weather API TLS is unverified
(`setInsecure`) so a MITM could show you wrong weather; WiFi credentials are
stored unencrypted in NVS flash (use ESP32 flash encryption if that matters
to you). There is no OTA endpoint — firmware changes require USB.

## Notes & tradeoffs

- The device stays awake so the config server is always reachable; expect to
  run it on USB power. (Deep-sleep-between-refreshes would stretch the
  battery to weeks but would make the web UI unreachable while sleeping —
  possible future toggle.)
- A full e-ink refresh takes several seconds with visible flashing. Normal
  for Spectra 6.
- Weather APIs are called over plain HTTP (nothing sensitive is sent) to
  avoid TLS certificate maintenance in firmware.

## Roadmap

- [ ] More themes; per-theme layout (not just palette)
- [ ] Hourly forecast view
- [ ] Optional deep-sleep battery mode
- [ ] Non-US postal code support (Zippopotam supports ~60 countries)
