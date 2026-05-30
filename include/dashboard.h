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
  /* MQTT topic rows */
  .mqtt-topic-row { display:flex; align-items:center; gap:0.5rem; flex-wrap:wrap; }
  .mqtt-dir { font-size:0.65rem; font-weight:700; padding:1px 6px; border-radius:4px; flex-shrink:0; }
  .mqtt-dir.pub { background:#dbeafe; color:#1d4ed8; }
  .mqtt-dir.sub { background:#dcfce7; color:#15803d; }
  .mqtt-topic { font-size:0.75rem; background:#f1f5f9; color:#334155;
                padding:2px 8px; border-radius:6px; border:1px solid #e2e8f0; word-break:break-all; }
  .mqtt-desc { font-size:0.72rem; color:#94a3b8; }
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

  <!-- ── DS18B20 TEMPERATURE ── -->
  <div class="card p-5">
    <div class="card-header">
      🌡 อุณหภูมิ DS18B20
      <span class="ml-auto" id="ds18-sim-badge"></span>
    </div>
    <div class="flex flex-col items-center justify-center py-4 gap-2">
      <div class="text-7xl font-bold text-rose-500 tracking-tight" id="ds18-temp">--</div>
      <div class="text-slate-400 text-sm">°C · GPIO 14</div>
      <div class="w-full mt-3">
        <div class="flex justify-between text-xs text-slate-400 mb-1.5">
          <span>ช่วงที่วัดได้</span><span>-55°C → 125°C</span>
        </div>
        <div class="gauge-track">
          <div class="gauge-bar bg-rose-400" id="ds18-bar" style="width:0%"></div>
        </div>
      </div>
    </div>
  </div>

  <!-- ── XYMD SENSOR ── -->
  <div class="card p-5">
    <div class="card-header">
      🌡 XY-MD03 (ID:2)
      <span class="ml-auto" id="xymd-sim-badge"></span>
    </div>
    <div class="grid grid-cols-2 gap-3">
      <div class="val-box">
        <div class="text-4xl font-bold text-teal-500" id="xymd-temp">--</div>
        <div class="text-xs text-slate-400 mt-1">อุณหภูมิ °C</div>
        <div class="gauge-track mt-2">
          <div class="gauge-bar bg-teal-400" id="xymd-temp-bar" style="width:0%"></div>
        </div>
      </div>
      <div class="val-box">
        <div class="text-4xl font-bold text-cyan-500" id="xymd-hum">--</div>
        <div class="text-xs text-slate-400 mt-1">ความชื้น %</div>
        <div class="gauge-track mt-2">
          <div class="gauge-bar bg-cyan-400" id="xymd-hum-bar" style="width:0%"></div>
        </div>
      </div>
      <div class="col-span-2 text-xs text-slate-400 text-center">
        Modbus RTU · Serial0 · 9600 8N1
      </div>
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

<!-- ═══ MQTT PANEL (full width) ═══ -->
<div class="card p-5 mt-5">
  <div class="card-header">
    🔗 MQTT Broker
    <span class="ml-2" id="mqtt-status-badge"></span>
    <span class="ml-auto normal-case font-normal text-slate-400 text-xs" id="mqtt-host-label">--</span>
  </div>
  <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
    <!-- Publish topics -->
    <div>
      <p class="text-xs font-bold text-slate-500 uppercase tracking-wide mb-2">📤 Publish Topics</p>
      <div class="space-y-1.5" id="mqtt-pub-topics">
        <div class="mqtt-topic-row">
          <span class="mqtt-dir pub">PUB</span>
          <code class="mqtt-topic" id="t-telemetry">--</code>
          <span class="mqtt-desc">ข้อมูลทั้งหมด (ทุก 5s)</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir pub">PUB</span>
          <code class="mqtt-topic" id="t-status">--</code>
          <span class="mqtt-desc">online / offline (LWT)</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir pub">PUB</span>
          <code class="mqtt-topic" id="t-r1-state">--</code>
          <span class="mqtt-desc">Relay 1 state</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir pub">PUB</span>
          <code class="mqtt-topic" id="t-r2-state">--</code>
          <span class="mqtt-desc">Relay 2 state</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir pub">PUB</span>
          <code class="mqtt-topic" id="t-r3-state">--</code>
          <span class="mqtt-desc">Relay 3 state</span>
        </div>
      </div>
    </div>
    <!-- Subscribe topics -->
    <div>
      <p class="text-xs font-bold text-slate-500 uppercase tracking-wide mb-2">📥 Subscribe Topics (Commands)</p>
      <div class="space-y-1.5">
        <div class="mqtt-topic-row">
          <span class="mqtt-dir sub">SUB</span>
          <code class="mqtt-topic" id="t-r1-set">--</code>
          <span class="mqtt-desc">ON / OFF / TOGGLE</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir sub">SUB</span>
          <code class="mqtt-topic" id="t-r2-set">--</code>
          <span class="mqtt-desc">ON / OFF / TOGGLE</span>
        </div>
        <div class="mqtt-topic-row">
          <span class="mqtt-dir sub">SUB</span>
          <code class="mqtt-topic" id="t-r3-set">--</code>
          <span class="mqtt-desc">ON / OFF / TOGGLE</span>
        </div>
        <div class="mt-3 p-3 bg-slate-50 rounded-lg border border-slate-200 text-xs text-slate-500 font-mono">
          <span class="font-bold text-slate-600">ตัวอย่าง:</span><br>
          mosquitto_pub -h broker.hivemq.com \<br>
          &nbsp;&nbsp;-t <span id="t-r1-set-ex" class="text-violet-600">--</span> -m ON
        </div>
      </div>
    </div>
  </div>
</div>

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

  // ── XYMD ──
  if (d.xymd !== undefined) {
    const xt = parseFloat(d.xymd.temp);
    const xh = parseFloat(d.xymd.hum);
    document.getElementById('xymd-temp').textContent = xt.toFixed(1);
    document.getElementById('xymd-hum').textContent  = xh.toFixed(1) + '%';
    document.getElementById('xymd-temp-bar').style.width =
      Math.max(0, Math.min(100, (xt / 50) * 100)) + '%';
    document.getElementById('xymd-hum-bar').style.width  =
      Math.max(0, Math.min(100, xh)) + '%';
    const xbadge = document.getElementById('xymd-sim-badge');
    xbadge.innerHTML = d.xymd.sim
      ? '<span class="text-xs font-semibold bg-amber-100 text-amber-700 px-2 py-0.5 rounded-full normal-case">SIM</span>'
      : '<span class="text-xs font-semibold bg-emerald-100 text-emerald-700 px-2 py-0.5 rounded-full normal-case">LIVE</span>';
  }

  // ── DS18B20 ──
  if (d.ds18 !== undefined) {
    const t = parseFloat(d.ds18.temp);
    document.getElementById('ds18-temp').textContent = t.toFixed(2);
    // progress bar: map -10°C→0%, 50°C→100%
    const pct = Math.max(0, Math.min(100, (t + 10) / 60 * 100));
    document.getElementById('ds18-bar').style.width = pct + '%';
    // simulation badge
    const badge = document.getElementById('ds18-sim-badge');
    if (d.ds18.sim) {
      badge.innerHTML = '<span class="text-xs font-semibold bg-amber-100 text-amber-700 px-2 py-0.5 rounded-full normal-case">SIM</span>';
    } else {
      badge.innerHTML = '<span class="text-xs font-semibold bg-emerald-100 text-emerald-700 px-2 py-0.5 rounded-full normal-case">LIVE</span>';
    }
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

  // ── MQTT ──
  if (d.mqtt) {
    const m = d.mqtt;
    document.getElementById('mqtt-host-label').textContent =
      m.host + ':' + m.port;
    const badge = document.getElementById('mqtt-status-badge');
    badge.innerHTML = m.connected
      ? '<span class="text-xs font-semibold bg-emerald-100 text-emerald-700 px-2 py-0.5 rounded-full">Connected</span>'
      : '<span class="text-xs font-semibold bg-red-100 text-red-600 px-2 py-0.5 rounded-full">Disconnected</span>';

    const set = (id, val) => {
      const el = document.getElementById(id);
      if (el) el.textContent = val;
    };
    set('t-telemetry', m.t_telemetry);
    set('t-status',    m.t_status);
    set('t-r1-state',  m.t_r1_state);
    set('t-r2-state',  m.t_r2_state);
    set('t-r3-state',  m.t_r3_state);
    set('t-r1-set',    m.t_r1_set);
    set('t-r2-set',    m.t_r2_set);
    set('t-r3-set',    m.t_r3_set);
    set('t-r1-set-ex', m.t_r1_set);
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
