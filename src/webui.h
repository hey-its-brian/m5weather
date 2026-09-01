#pragma once

// Config page served at "/". Self-contained: no external assets, works on the
// LAN and in captive-portal (AP) mode. Kept as one raw literal in PROGMEM.
static const char WEBUI_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>M5Weather</title>
<style>
  :root {
    --bg: #f4f1ea; --card: #ffffff; --ink: #1a1a1a; --sub: #6b6b6b;
    --accent: #c0392b; --line: #e2ddd2; --ok: #1e7d3c; --err: #c0392b;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: Georgia, 'Times New Roman', serif;
    background: var(--bg); color: var(--ink);
    max-width: 560px; margin: 0 auto; padding: 24px 16px 48px;
  }
  h1 { font-size: 1.7rem; letter-spacing: .02em; }
  h1 small { font-size: .95rem; color: var(--sub); font-weight: normal; margin-left: 8px; }
  .rule { height: 3px; background: var(--accent); width: 72px; margin: 10px 0 22px; }
  .card {
    background: var(--card); border: 1px solid var(--line); border-radius: 10px;
    padding: 18px 20px; margin-bottom: 18px;
  }
  .card h2 { font-size: 1.05rem; margin-bottom: 14px; color: var(--ink); }
  label { display: block; font-size: .85rem; color: var(--sub); margin: 12px 0 4px; }
  input, select {
    width: 100%; padding: 10px 12px; font-size: 1rem; font-family: inherit;
    border: 1px solid var(--line); border-radius: 6px; background: #fdfcf9; color: var(--ink);
  }
  input:focus, select:focus { outline: 2px solid var(--accent); border-color: transparent; }
  .row { display: flex; gap: 12px; }
  .row > div { flex: 1; }
  button {
    margin-top: 18px; width: 100%; padding: 12px; font-size: 1rem; font-family: inherit;
    background: var(--accent); color: #fff; border: 0; border-radius: 6px; cursor: pointer;
  }
  button.secondary { background: transparent; color: var(--accent); border: 1px solid var(--accent); }
  button:disabled { opacity: .5; cursor: default; }
  #msg { margin-top: 12px; font-size: .9rem; min-height: 1.2em; }
  #msg.ok { color: var(--ok); } #msg.err { color: var(--err); }
  dl { display: grid; grid-template-columns: auto 1fr; gap: 6px 16px; font-size: .92rem; }
  dt { color: var(--sub); } dd { text-align: right; }
  .note { font-size: .8rem; color: var(--sub); margin-top: 10px; line-height: 1.4; }
</style>
</head>
<body>
  <h1>M5Weather <small>PaperColor</small></h1>
  <div class="rule"></div>

  <div class="card">
    <h2>Location &amp; Display</h2>
    <label for="zip">Zip code</label>
    <input id="zip" inputmode="numeric" pattern="[0-9]{5}" maxlength="5" placeholder="e.g. 30301">
    <div class="row">
      <div>
        <label for="units">Units</label>
        <select id="units">
          <option value="imperial">&deg;F, mph</option>
          <option value="metric">&deg;C, km/h</option>
        </select>
      </div>
      <div>
        <label for="refresh">Refresh every</label>
        <select id="refresh">
          <option value="15">15 minutes</option>
          <option value="30">30 minutes</option>
          <option value="60">1 hour</option>
          <option value="180">3 hours</option>
        </select>
      </div>
    </div>
    <label for="theme">Theme</label>
    <select id="theme"></select>
    <button id="save">Save &amp; Update Display</button>
    <div id="msg"></div>
  </div>

  <div class="card">
    <h2>Wi-Fi</h2>
    <label for="ssid">Network name (SSID)</label>
    <input id="ssid" autocomplete="off">
    <label for="pass">Password</label>
    <input id="pass" type="password" autocomplete="off" placeholder="(unchanged)">
    <button id="savewifi" class="secondary">Save Wi-Fi &amp; Reboot</button>
    <p class="note">The device reboots and joins this network. If it can't connect,
    it reopens the <b>M5Weather-Setup</b> hotspot so you can try again.</p>
  </div>

  <div class="card">
    <h2>Status</h2>
    <dl>
      <dt>Location</dt><dd id="st-place">-</dd>
      <dt>Last update</dt><dd id="st-updated">-</dd>
      <dt>Indoor</dt><dd id="st-room">-</dd>
      <dt>Battery</dt><dd id="st-batt">-</dd>
      <dt>Wi-Fi signal</dt><dd id="st-rssi">-</dd>
      <dt>Address</dt><dd id="st-addr">-</dd>
    </dl>
    <button id="refreshnow" class="secondary">Refresh Weather Now</button>
  </div>

<script>
const $ = id => document.getElementById(id);
const msg = (text, cls) => { const m = $('msg'); m.textContent = text; m.className = cls || ''; };

async function load() {
  try {
    const c = await (await fetch('/api/config')).json();
    $('zip').value = c.zip || '';
    $('units').value = c.units;
    $('refresh').value = String(c.refresh_minutes);
    $('theme').innerHTML = c.themes.map(t =>
      `<option value="${t.id}">${t.label}</option>`).join('');
    $('theme').value = c.theme;
    $('ssid').value = c.wifi_ssid || '';
  } catch (e) { msg('Could not load settings: ' + e, 'err'); }
  loadStatus();
}

async function loadStatus() {
  try {
    const s = await (await fetch('/api/status')).json();
    $('st-place').textContent = s.place || '(not set)';
    $('st-updated').textContent = s.last_update || 'never';
    if (s.room_temp_c !== undefined) {
      const f = Math.round(s.room_temp_c * 9 / 5 + 32);
      $('st-room').textContent = f + '\u00b0F / ' + s.room_temp_c.toFixed(1) +
        '\u00b0C, ' + s.room_humidity + '% RH';
    } else { $('st-room').textContent = 'no sensor'; }
    $('st-batt').textContent = s.battery_pct >= 0 ? s.battery_pct + '%' : 'n/a';
    $('st-rssi').textContent = s.rssi ? s.rssi + ' dBm' : 'n/a';
    $('st-addr').textContent = s.address || '';
  } catch (e) { /* status is best-effort */ }
}

$('save').onclick = async () => {
  const zip = $('zip').value.trim();
  if (!/^[0-9]{5}$/.test(zip)) { msg('Enter a 5-digit zip code.', 'err'); return; }
  $('save').disabled = true;
  msg('Saving... the e-ink display takes a few seconds to redraw.');
  try {
    const r = await fetch('/api/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({
        zip, units: $('units').value,
        refresh_minutes: parseInt($('refresh').value, 10),
        theme: $('theme').value,
      }),
    });
    const j = await r.json();
    if (r.ok) { msg('Saved. Display updating for ' + (j.place || zip) + '.', 'ok'); loadStatus(); }
    else msg(j.error || 'Save failed.', 'err');
  } catch (e) { msg('Save failed: ' + e, 'err'); }
  $('save').disabled = false;
};

$('savewifi').onclick = async () => {
  const ssid = $('ssid').value.trim();
  if (!ssid) { msg('Enter a network name.', 'err'); return; }
  $('savewifi').disabled = true;
  try {
    await fetch('/api/wifi', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ssid, pass: $('pass').value}),
    });
    msg('Wi-Fi saved. Rebooting - reconnect at http://m5weather.local', 'ok');
  } catch (e) { msg('Failed: ' + e, 'err'); $('savewifi').disabled = false; }
};

$('refreshnow').onclick = async () => {
  $('refreshnow').disabled = true;
  msg('Refreshing weather...');
  try {
    const r = await fetch('/api/refresh', {method: 'POST'});
    const j = await r.json();
    if (r.ok) { msg('Weather refreshed.', 'ok'); loadStatus(); }
    else msg(j.error || 'Refresh failed.', 'err');
  } catch (e) { msg('Refresh failed: ' + e, 'err'); }
  $('refreshnow').disabled = false;
};

load();
</script>
</body>
</html>)HTML";
