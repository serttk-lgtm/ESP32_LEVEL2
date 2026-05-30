#ifndef DASHBOARD_H
#define DASHBOARD_H

// Dashboard HTML/CSS/JS stored in flash (PROGMEM)
// Uses Tailwind CDN + vanilla JS WebSocket — no build step needed

static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Dashboard</title>
<script src="https://cdn.tailwindcss.com"></script>
<style>
  body {
    background: linear-gradient(135deg, #e0f2fe 0%, #f0fdf4 50%, #fef9c3 100%);
    font-family: 'Segoe UI', sans-serif;
    min-height: 100vh;
  }
  .card {
    background: #ffffff;
    border: 1px solid #e2e8f0;
    border-radius: 16px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.07);
  }
  .card-header {
    font-size: 0.85rem;
    font-weight: 700;
    color: #64748b;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    margin-bottom: 1rem;
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  /* Relay rows */
  .relay-on  { background: #f0fdf4; border: 1.5px solid #86efac; }
  .relay-off { background: #f8fafc; border: 1.5px solid #e2e8f0; }
  .btn-on  { background: #16a34a; color: #fff; }
  .btn-on:hover  { background: #15803d; }
  .btn-off { background: #f1f5f9; color: #475569; border: 1px solid #cbd5e1; }
  .btn-off:hover { background: #e2e8f0; }
  /* Weather value boxes */
  .val-box {
    background: #f8fafc;
    border: 1px solid #e2e8f0;
    border-radius: 12px;
    padding: 0.75rem;
    text-align: center;
  }
  /* WiFi rows */
  .info-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem 0;
    border-bottom: 1px solid #f1f5f9;
  }
  .info-row:last-child { border-bottom: none; }
  .info-label { color: #94a3b8; font-size: 0.82rem; }
  .info-value { color: #1e293b; font-family: monospace; font-size: 0.85rem; font-weight: 600; }
  /* Live badge */
  .badge-live { animation: pulse 2s infinite; }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.35} }
  /* AQI colors (on light bg) */
  .aqi-1{color:#16a34a} .aqi-2{color:#65a30d} .aqi-3{color:#ca8a04}
  .aqi-4{color:#ea580c} .aqi-5{color:#dc2626}
  /* Progress bar */
  .gauge-track { background:#e2e8f0; border-radius:999px; height:6px; overflow:hidden; }
  .gauge-bar   { height:6px; border-radius:999px; transition:width 0.6s ease; }
</style>
</head>
<body class="p-4 md:p-6">

<!-- ═══ HEADER ═══ -->
<div class="flex flex-wrap items-center justify-between mb-6 gap-3">
  <div>
    <h1 class="text-2xl font-bold text-slate-800 tracking-tight">
      ⚙️ ESP32 Dashboard
    </h1>
    <p class="text-sky-600 text-sm mt-0.5 font-mono" id="ip-label">กำลังเชื่อมต่อ...</p>
  </div>
  <div class="flex items-center gap-2 bg-white rounded-full px-4 py-2 shadow-sm border border-slate-200">
    <span class="badge-live w-2.5 h-2.5 rounded-full bg-emerald-400" id="live-dot"></span>
    <span class="text-sm font-semibold text-slate-600" id="live-label">Connecting</span>
    <span class="text-xs text-slate-400 ml-1" id="last-update"></span>
  </div>
</div>

<!-- ═══ GRID ═══ -->
<div class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-5">

  <!-- ── RELAY CONTROL ── -->
  <div class="card p-5">
    <div class="card-header">⚡ Relay Control</div>
    <div class="space-y-3" id="relay-container">
      <!-- injected by JS -->
    </div>
  </div>

  <!-- ── OPEN WEATHER ── -->
  <div class="card p-5">
    <div class="card-header">
      🌤 สภาพอากาศ
      <span class="ml-auto normal-case font-normal text-slate-400 text-xs">Nakhon Si Thammarat</span>
    </div>
    <div class="grid grid-cols-2 gap-3">
      <!-- Temp -->
      <div class="val-box">
        <div class="text-3xl font-bold text-orange-500" id="w-temp">--</div>
        <div class="text-xs text-slate-400 mt-1">อุณหภูมิ °C</div>
      </div>
      <!-- Humidity -->
      <div class="val-box">
        <div class="text-3xl font-bold text-sky-500" id="w-hum">--</div>
        <div class="text-xs text-slate-400 mt-1">ความชื้น %</div>
      </div>
      <!-- Rain -->
      <div class="col-span-2 val-box">
        <div class="flex justify-between text-sm mb-2">
          <span class="text-slate-500 font-medium">🌧 โอกาสฝนตก</span>
          <span class="text-sky-600 font-bold" id="w-rain-pct">--%</span>
        </div>
        <div class="gauge-track">
          <div class="gauge-bar bg-sky-400" id="w-rain-bar" style="width:0%"></div>
        </div>
      </div>
      <!-- AQI -->
      <div class="val-box">
        <div class="text-2xl font-bold" id="w-aqi-label">--</div>
        <div class="text-xs text-slate-400 mt-1">AQI · <span id="w-aqi-num">-</span></div>
      </div>
      <!-- PM2.5 -->
      <div class="val-box">
        <div class="text-2xl font-bold text-amber-500" id="w-pm25">--</div>
        <div class="text-xs text-slate-400 mt-1">PM2.5 µg/m³</div>
      </div>
    </div>
  </div>

  <!-- ── WIFI & SYSTEM ── -->
  <div class="card p-5">
    <div class="card-header">📶 WiFi &amp; Network</div>
    <div class="space-y-0.5">
      <div class="info-row">
        <span class="info-label">SSID</span>
        <span class="info-value text-slate-700" id="wifi-ssid">--</span>
      </div>
      <div class="info-row">
        <span class="info-label">IP Address</span>
        <span class="info-value text-emerald-600" id="wifi-ip">--</span>
      </div>
      <div class="info-row">
        <span class="info-label">RSSI</span>
        <span class="info-value" id="wifi-rssi-text">--</span>
      </div>
      <div class="info-row">
        <span class="info-label">MAC Address</span>
        <span class="info-value text-slate-500 text-xs" id="wifi-mac">--</span>
      </div>
      <div class="info-row">
        <span class="info-label">Free Heap</span>
        <span class="info-value text-violet-600" id="sys-heap">-- KB</span>
      </div>
      <div class="info-row">
        <span class="info-label">Uptime</span>
        <span class="info-value text-slate-600" id="sys-uptime">--</span>
      </div>
    </div>
    <!-- Signal bar -->
    <div class="mt-4">
      <div class="flex justify-between text-xs text-slate-400 mb-1.5">
        <span>Signal Strength</span>
        <span id="wifi-rssi-val">-- dBm</span>
      </div>
      <div class="gauge-track">
        <div class="gauge-bar" id="wifi-signal-bar" style="width:0%"></div>
      </div>
    </div>
  </div>

</div><!-- end grid -->

<p class="text-center text-slate-400 text-xs mt-6">
  ESP32 Local Dashboard · Real-time via WebSocket
</p>

<script>
// ─── WebSocket ───────────────────────────────────────────────────
const wsUrl = `ws://${location.hostname}/ws`;
let ws, reconnectTimer;

function connect() {
  ws = new WebSocket(wsUrl);
  ws.onopen    = () => { setLive(true);  clearTimeout(reconnectTimer); };
  ws.onclose   = () => { setLive(false); reconnectTimer = setTimeout(connect, 3000); };
  ws.onerror   = () => ws.close();
  ws.onmessage = (e) => { try { render(JSON.parse(e.data)); } catch(_){} };
}

function setLive(ok) {
  document.getElementById('live-dot').className =
    `badge-live w-2.5 h-2.5 rounded-full ${ok ? 'bg-emerald-400' : 'bg-red-400'}`;
  document.getElementById('live-label').textContent = ok ? 'Live' : 'Disconnected';
}

// ─── Render ──────────────────────────────────────────────────────
function render(d) {
  document.getElementById('last-update').textContent =
    new Date().toLocaleTimeString('th-TH');

  // ── Relay ──
  if (d.relay) {
    const rc = document.getElementById('relay-container');
    rc.innerHTML = '';
    d.relay.forEach((on, i) => {
      const n = i + 1;
      const row = document.createElement('div');
      row.className = `flex items-center justify-between rounded-xl px-4 py-3 transition-all ${on ? 'relay-on' : 'relay-off'}`;
      row.innerHTML = `
        <div class="flex items-center gap-3">
          <div class="w-9 h-9 rounded-full flex items-center justify-center text-lg
            ${on ? 'bg-emerald-100' : 'bg-slate-100'}">
            ${on ? '🟢' : '⚫'}
          </div>
          <div>
            <div class="font-semibold text-slate-800">Relay ${n}</div>
            <div class="text-xs ${on ? 'text-emerald-600' : 'text-slate-400'} font-medium">
              ${on ? 'เปิดอยู่' : 'ปิดอยู่'}
            </div>
          </div>
        </div>
        <button onclick="toggleRelay(${n})"
          class="px-5 py-1.5 rounded-lg text-sm font-bold transition-all ${on ? 'btn-on' : 'btn-off'}">
          ${on ? 'ปิด' : 'เปิด'}
        </button>`;
      rc.appendChild(row);
    });
  }

  // ── Weather ──
  if (d.weather && d.weather.valid) {
    const w = d.weather;
    document.getElementById('w-temp').textContent     = w.temp.toFixed(1);
    document.getElementById('w-hum').textContent      = w.hum + '%';
    document.getElementById('w-rain-pct').textContent = w.rain + '%';
    document.getElementById('w-rain-bar').style.width = w.rain + '%';
    document.getElementById('w-pm25').textContent     = w.pm25.toFixed(1);
    document.getElementById('w-aqi-num').textContent  = w.aqi;
    const aqiEl = document.getElementById('w-aqi-label');
    aqiEl.textContent = w.aqiLabel;
    aqiEl.className   = `text-2xl font-bold aqi-${w.aqi}`;
  }

  // ── WiFi ──
  if (d.wifi) {
    const wf = d.wifi;
    document.getElementById('wifi-ssid').textContent = wf.ssid || '--';
    document.getElementById('wifi-ip').textContent   = wf.ip   || '--';
    document.getElementById('wifi-mac').textContent  = wf.mac  || '--';
    document.getElementById('ip-label').textContent  = 'http://' + (wf.ip || '...');

    const rssi  = wf.rssi || -100;
    const pct   = Math.max(0, Math.min(100, (rssi + 100) * 2));
    const tColor = pct > 60 ? 'text-emerald-600' : pct > 30 ? 'text-amber-500' : 'text-red-500';
    const bColor = pct > 60 ? 'bg-emerald-400'  : pct > 30 ? 'bg-amber-400'   : 'bg-red-400';

    document.getElementById('wifi-rssi-val').textContent  = rssi + ' dBm';
    document.getElementById('wifi-rssi-text').textContent = rssi + ' dBm';
    document.getElementById('wifi-rssi-text').className   = `info-value font-mono ${tColor}`;
    const bar = document.getElementById('wifi-signal-bar');
    bar.style.width = pct + '%';
    bar.className   = `gauge-bar ${bColor}`;
  }

  // ── System ──
  if (d.sys) {
    document.getElementById('sys-heap').textContent   = (d.sys.heap / 1024).toFixed(1) + ' KB';
    document.getElementById('sys-uptime').textContent = fmtUptime(d.sys.uptime);
  }
}

// ─── Actions ─────────────────────────────────────────────────────
function toggleRelay(n) {
  if (ws && ws.readyState === WebSocket.OPEN)
    ws.send(JSON.stringify({ cmd: 'relay', n }));
}

// ─── Helpers ─────────────────────────────────────────────────────
function fmtUptime(sec) {
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  return `${h}h ${m}m ${s}s`;
}

connect();
</script>
</body>
</html>
)rawhtml";

#endif // DASHBOARD_H
