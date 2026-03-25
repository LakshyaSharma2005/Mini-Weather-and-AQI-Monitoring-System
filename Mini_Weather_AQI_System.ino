#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <time.h>
#include <Firebase_ESP_Client.h>

#define API_KEY "API_KEY"
#define DATABASE_URL "DATABASE_URL"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ2_PIN 34
#define LED_PIN 2

// --- RAIN SENSOR PINS ---
#define RAIN_AO_PIN 35
#define RAIN_DO_PIN 26

const char* ssid = "realme";
const char* password = "11111111";

// --- TIME CONFIG ---
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";

WebServer server(80);
int aqiThreshold = 150;   

// --- TIMERS ---
unsigned long lastSensorMillis = 0;
const unsigned long sensorInterval = 2000; // UI update interval
unsigned long lastLogMillis = 0;
const unsigned long logInterval = 60000; // Save to CSV every 60 seconds

float lastTemp = NAN;
float lastHum  = NAN;
int   lastMq2  = 0;
int   lastAqi  = 0;
int   lastRainPct = 0;
bool  lastIsRaining = false;
const char* csvPath = "/data.csv";

int calculateAQI(int mqValue);

// --- Helpers ---
String getCurrentDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "No-Sync";
  char buf[16];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  return String(buf);
}
String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return String(millis());
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

// --- CSV Logic ---
void ensureCsvHeader() {
  if (SPIFFS.exists(csvPath)) return; 
  File f = SPIFFS.open(csvPath, FILE_WRITE);
  if (f) {
    f.println("Date,Time,Temperature,Humidity,AQI,RainPct,AQI_Alert,Rain_Alert");
    f.close();
    Serial.println("-> CSV Header created.");
  }
}

void saveReadingToCSV(float t, float h, int m, int r, bool aqiAlert, bool rainAlert) {
  File f = SPIFFS.open(csvPath, FILE_APPEND);
  if (!f) return;
  String line = getCurrentDate() + "," + getCurrentTime() + "," + 
                (isnan(t)?"0":String(t,1)) + "," + 
                (isnan(h)?"0":String(h,1)) + "," + 
                String(m) + "," + String(r) + "," + 
                (aqiAlert?"1":"0") + "," + (rainAlert?"1":"0");
  f.println(line);
  f.close();
  Serial.println("-> Data logged to CSV.");
}

// --- Dashboard (GSAP + Canvas UI + System Log + Section Labels) ---
// --- Dashboard (GSAP + Canvas UI + System Log + Section Labels) ---
String getDashboardPage() {
  return R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SkyCast: Environment Monitoring and Analysis</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root {
  --bg: #060a12;
  --surface: #0d1525;
  --surface2: #111d30;
  --border: rgba(99,179,237,0.12);
  --border2: rgba(99,179,237,0.22);
  --text: #e8edf5;
  --muted: #5a7498;
  --cyan: #38bdf8;
  --cyan-dim: rgba(56,189,248,0.15);
  --green: #34d399;
  --green-dim: rgba(52,211,153,0.15);
  --amber: #fbbf24;
  --amber-dim: rgba(251,191,36,0.15);
  --pink: #f472b6;
  --pink-dim: rgba(244,114,182,0.15);
  --red: #f87171;
  --red-dim: rgba(248,113,113,0.15);
  --blue: #3b82f6;
  --blue-dim: rgba(59,130,246,0.15);
  --font: 'Space Grotesk', sans-serif;
  --mono: 'JetBrains Mono', monospace;
}
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
html { scroll-behavior: smooth; }
body {
  font-family: var(--font);
  background: var(--bg);
  color: var(--text);
  min-height: 100vh;
  overflow-x: hidden;
}
#particle-canvas {
  position: fixed; top: 0; left: 0; width: 100%; height: 100%;
  pointer-events: none; z-index: 0; opacity: 0.5;
}
.app { position: relative; z-index: 1; max-width: 1380px; margin: 0 auto; padding: 24px 20px 60px; }

/* ─── HEADER ─── */
.header {
  display: flex; justify-content: space-between; align-items: flex-start;
  margin-bottom: 24px; padding-bottom: 24px;
  border-bottom: 1px solid var(--border);
  flex-wrap: wrap; gap: 16px;
}
.brand { display: flex; align-items: center; gap: 14px; }
.brand-icon {
  width: 44px; height: 44px; border-radius: 12px;
  background: linear-gradient(135deg, #0ea5e9, #38bdf8);
  display: flex; align-items: center; justify-content: center;
  box-shadow: 0 0 20px rgba(56,189,248,0.35);
  flex-shrink: 0;
}
.brand-icon svg { width: 24px; height: 24px; }
.brand-text h1 {
  font-size: 22px; font-weight: 700; letter-spacing: -0.5px;
  background: linear-gradient(90deg, #e8edf5 20%, #38bdf8 80%);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent;
}
.brand-text .sub {
  font-size: 12px; color: var(--muted); font-family: var(--mono);
  margin-top: 2px; display: flex; align-items: center; gap: 6px;
}
.live-dot {
  width: 6px; height: 6px; border-radius: 50%; background: var(--green);
  animation: pulse-dot 2s ease-in-out infinite;
}
@keyframes pulse-dot {
  0%, 100% { box-shadow: 0 0 0 0 rgba(52,211,153,0.5); }
  50% { box-shadow: 0 0 0 5px rgba(52,211,153,0); }
}
.header-controls { display: flex; gap: 10px; flex-wrap: wrap; }
.btn {
  font-family: var(--font); font-size: 12px; font-weight: 600;
  padding: 9px 16px; border-radius: 8px; cursor: pointer;
  border: 1px solid var(--border2); background: var(--surface);
  color: var(--text); letter-spacing: 0.3px; transition: all 0.2s;
}
.btn:hover { background: var(--surface2); border-color: rgba(99,179,237,0.4); }
.btn.accent { background: rgba(56,189,248,0.15); border-color: rgba(56,189,248,0.5); color: var(--cyan); }
.btn.accent:hover { background: rgba(56,189,248,0.25); box-shadow: 0 0 16px rgba(56,189,248,0.2); }
.btn.danger { color: var(--red); border-color: rgba(248,113,113,0.3); }
.btn.danger:hover { background: var(--red-dim); }

/* ─── SECTION TITLES ─── */
.section-title {
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 2px;
  text-transform: uppercase;
  color: var(--cyan);
  margin: 32px 0 16px 4px;
  display: flex;
  align-items: center;
  gap: 16px;
  opacity: 0.9;
}
.section-title::after {
  content: '';
  flex: 1;
  height: 1px;
  background: linear-gradient(90deg, var(--border2), transparent);
}
.section-icon { font-size: 16px; }

/* ─── GRID ─── */
.grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin-bottom: 20px; }
.grid-2 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin-bottom: 20px; }
.grid-charts { display: grid; grid-template-columns: repeat(2, 1fr); gap: 20px; margin-bottom: 20px; }

/* ─── CARD BASE ─── */
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 16px;
  position: relative;
  overflow: hidden;
  transition: border-color 0.3s;
}
.card::before {
  content: ''; position: absolute; inset: 0; border-radius: 16px;
  background: linear-gradient(135deg, rgba(255,255,255,0.02) 0%, transparent 60%);
  pointer-events: none;
}
.card:hover { border-color: var(--border2); }

/* ─── STAT CARDS ─── */
.stat-card { padding: 24px; }
.stat-top { display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 16px; }
.stat-label {
  font-size: 11px; font-weight: 600; letter-spacing: 1.5px;
  text-transform: uppercase; color: var(--muted);
}
.stat-icon {
  width: 36px; height: 36px; border-radius: 10px;
  display: flex; align-items: center; justify-content: center;
  font-size: 16px;
}
.stat-val {
  font-size: 44px; font-weight: 700; letter-spacing: -2px;
  line-height: 1; font-family: var(--mono); margin-bottom: 8px;
}
.stat-unit { font-size: 20px; font-weight: 400; opacity: 0.6; margin-left: 2px; }
.stat-sub { font-size: 12px; color: var(--muted); display: flex; align-items: center; gap: 6px; }
.stat-badge {
  display: inline-block; padding: 2px 8px; border-radius: 20px;
  font-size: 10px; font-weight: 700; letter-spacing: 0.5px;
}

/* Colors */
.card-temp .stat-icon { background: var(--cyan-dim); color: var(--cyan); }
.card-temp .stat-val { color: var(--cyan); }
.card-temp .top-line { position: absolute; top: 0; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, var(--cyan), transparent); }

.card-hum .stat-icon { background: var(--pink-dim); color: var(--pink); }
.card-hum .stat-val { color: var(--pink); }
.card-hum .top-line { position: absolute; top: 0; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, var(--pink), transparent); }

.card-aqi .top-line { position: absolute; top: 0; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, var(--amber), transparent); }

.card-rain .stat-icon { background: var(--blue-dim); color: var(--blue); }
.card-rain .stat-val { color: var(--blue); }
.card-rain .top-line { position: absolute; top: 0; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, var(--blue), transparent); }


/* ─── AQI GAUGE (FIXED FOR RESPONSIVENESS) ─── */
.aqi-gauge-wrap { display: flex; align-items: center; gap: 12px; }
.gauge-svg-wrap { position: relative; width: 100px; height: 58px; flex-shrink: 0; }
.gauge-svg-wrap svg { overflow: visible; width: 100%; height: 100%; }
#gauge-val-overlay {
  position: absolute; bottom: -2px; left: 0; right: 0;
  text-align: center; font-size: 13px; font-weight: 700;
  font-family: var(--mono); color: var(--amber);
}
.aqi-details { flex: 1; min-width: 0; }
.aqi-level {
  font-size: 14px; 
  font-weight: 700; 
  margin-bottom: 4px;
  transition: color 0.5s;
  line-height: 1.2;
  display: -webkit-box;
  -webkit-line-clamp: 2; /* Limits text to 2 lines so it doesn't push the slider too far down */
  -webkit-box-orient: vertical;
  overflow: hidden;
}
.aqi-threshold { font-size: 11px; color: var(--muted); margin-bottom: 10px; white-space: nowrap; }
.threshold-row { display: flex; align-items: center; gap: 8px; }
.threshold-row input[type=range] {
  flex: 1; height: 4px; background: var(--surface2); border-radius: 2px;
  outline: none; -webkit-appearance: none; min-width: 0;
}
.threshold-row input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none; width: 14px; height: 14px;
  background: var(--amber); border-radius: 50%; cursor: pointer;
  box-shadow: 0 0 8px rgba(251,191,36,0.5);
}
.threshold-save { padding: 4px 10px; font-size: 10px; border-radius: 6px; flex-shrink: 0; }

/* ─── ANALYTICS CARDS ─── */
.analytics-card { padding: 22px; }
.analytics-header {
  display: flex; align-items: center; gap: 8px; margin-bottom: 18px;
}
.analytics-dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
.analytics-title { font-size: 12px; font-weight: 600; letter-spacing: 1px; text-transform: uppercase; color: var(--muted); }
.stat-row {
  display: flex; justify-content: space-between; align-items: center;
  padding: 8px 0; border-bottom: 1px solid rgba(255,255,255,0.04);
  font-size: 13px;
}
.stat-row:last-child { border: none; }
.stat-row-label { color: var(--muted); }
.stat-row-val { font-family: var(--mono); font-weight: 600; font-size: 14px; }

/* ─── CHART CARDS ─── */
.chart-card { padding: 20px; }
.chart-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
.chart-title { font-size: 11px; font-weight: 600; letter-spacing: 1.2px; text-transform: uppercase; color: var(--muted); }
.chart-current { font-family: var(--mono); font-size: 14px; font-weight: 600; }
.chart-wrap { position: relative; height: 140px; }
.chart-wrap canvas { width: 100% !important; }

/* ─── SYSTEM LOG ─── */
.log-card { padding: 20px; }
.log-box {
  height: 160px; overflow-y: auto; font-family: var(--mono);
  font-size: 12px; color: var(--muted); background: rgba(0,0,0,0.3);
  padding: 14px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.03);
}
.log-entry { margin-bottom: 6px; border-bottom: 1px solid rgba(255,255,255,0.03); padding-bottom: 6px; }
.log-entry:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }
.log-time { color: var(--cyan); margin-right: 10px; }
.log-msg { color: var(--text); }
.log-alert { color: var(--red); font-weight: 600; }

/* Custom Scrollbar for Log */
.log-box::-webkit-scrollbar { width: 6px; }
.log-box::-webkit-scrollbar-track { background: transparent; }
.log-box::-webkit-scrollbar-thumb { background: var(--surface2); border-radius: 4px; }
.log-box::-webkit-scrollbar-thumb:hover { background: var(--muted); }

/* ─── ALERT BANNER ─── */
.alert-banner {
  display: none; align-items: center; gap: 12px; padding: 14px 20px;
  background: rgba(248,113,113,0.08); border: 1px solid rgba(248,113,113,0.3);
  border-radius: 12px; margin-bottom: 20px;
  animation: alert-flash 2s ease-in-out infinite;
}
.alert-banner.show { display: flex; }
@keyframes alert-flash {
  0%, 100% { box-shadow: 0 0 0 0 rgba(248,113,113,0); }
  50% { box-shadow: 0 0 20px rgba(248,113,113,0.2); }
}
.alert-icon { font-size: 20px; }
.alert-text { font-size: 13px; font-weight: 600; color: var(--red); }

/* ─── FOOTER ─── */
.footer {
  margin-top: 36px; padding-top: 20px; border-top: 1px solid var(--border);
  display: flex; justify-content: space-between; align-items: center;
  flex-wrap: wrap; gap: 12px;
  font-size: 11px; color: var(--muted); font-family: var(--mono);
}
.footer-right { display: flex; gap: 16px; }

@media (max-width: 1200px) {
  .grid, .grid-2 { grid-template-columns: repeat(2, 1fr); }
}
@media (max-width: 900px) {
  .grid, .grid-2, .grid-charts { grid-template-columns: 1fr; }
}
@media (max-width: 600px) {
  .stat-val { font-size: 36px; }
  .brand-text h1 { font-size: 18px; }
}
</style>
</head>
<body>

<canvas id="particle-canvas"></canvas>

<div class="app">
  <header class="header">
    <div class="brand">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M12 2L2 7l10 5 10-5-10-5z"/><path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/>
        </svg>
      </div>
      <div class="brand-text">
        <h1>SkyCast: Environment Monitoring and Analysis</h1>
        <div class="sub"><div class="live-dot"></div><span id="ip-addr">Connecting...</span> &nbsp;·&nbsp; <span id="clock">--:--:--</span></div>
      </div>
    </div>
    <div class="header-controls">
      <button class="btn" id="btnRefreshStats">↻ Refresh</button>
      <button class="btn accent" id="btnExport">↓ Export CSV</button>
      <button class="btn danger" id="btnClear">⊗ Clear Data</button>
    </div>
  </header>

  <div class="alert-banner" id="alertBanner">
    <div class="alert-icon">⚠</div>
    <div class="alert-text">ENVIRONMENTAL ALERT — Critical threshold exceeded or Rain detected. Take precautionary action.</div>
  </div>

  <div class="section-title"><span class="section-icon">⚡</span> Real-Time Metrics</div>
  <div class="grid">
    <div class="card stat-card card-temp">
      <div class="top-line"></div>
      <div class="stat-top">
        <div class="stat-label">Temperature</div>
        <div class="stat-icon">🌡</div>
      </div>
      <div class="stat-val"><span id="temp">--</span><span class="stat-unit">°C</span></div>
      <div class="stat-sub">
        <span class="stat-badge" id="temp-badge" style="background:var(--cyan-dim);color:var(--cyan)">LIVE</span>
        <span>Ambient reading</span>
      </div>
    </div>

    <div class="card stat-card card-hum">
      <div class="top-line"></div>
      <div class="stat-top">
        <div class="stat-label">Humidity</div>
        <div class="stat-icon">💧</div>
      </div>
      <div class="stat-val"><span id="hum">--</span><span class="stat-unit">%</span></div>
      <div class="stat-sub">
        <span class="stat-badge" id="hum-badge" style="background:var(--pink-dim);color:var(--pink)">LIVE</span>
        <span>Relative humidity</span>
      </div>
    </div>

    <div class="card stat-card card-rain">
      <div class="top-line"></div>
      <div class="stat-top">
        <div class="stat-label">Rain Intensity</div>
        <div class="stat-icon">🌧</div>
      </div>
      <div class="stat-val"><span id="rain">--</span><span class="stat-unit">%</span></div>
      <div class="stat-sub">
        <span class="stat-badge" id="rain-badge" style="background:var(--blue-dim);color:var(--blue)">DRY</span>
        <span>Water detection</span>
      </div>
    </div>

    <div class="card stat-card card-aqi">
      <div class="top-line"></div>
      <div class="stat-top">
        <div class="stat-label">Air Quality Index</div>
      </div>
      <div class="aqi-gauge-wrap">
        <div class="gauge-svg-wrap">
          <svg viewBox="0 0 120 68" width="100%" height="100%">
            <defs>
              <linearGradient id="gaugeGrad" x1="0%" y1="0%" x2="100%" y2="0%">
                <stop offset="0%" stop-color="#34d399"/>
                <stop offset="40%" stop-color="#fbbf24"/>
                <stop offset="75%" stop-color="#f97316"/>
                <stop offset="100%" stop-color="#ef4444"/>
              </linearGradient>
            </defs>
            <path d="M10,60 A50,50 0 0,1 110,60" fill="none" stroke="rgba(255,255,255,0.06)" stroke-width="10" stroke-linecap="round"/>
            <path id="gauge-track" d="M10,60 A50,50 0 0,1 110,60" fill="none" stroke="url(#gaugeGrad)" stroke-width="10" stroke-linecap="round" stroke-dasharray="157" stroke-dashoffset="157" style="transition: stroke-dashoffset 1s cubic-bezier(.4,0,.2,1);"/>
            <circle id="gauge-needle" cx="10" cy="60" r="5" fill="var(--amber)" style="filter:drop-shadow(0 0 4px #fbbf24); transition: all 1s cubic-bezier(.4,0,.2,1);"/>
          </svg>
          <div id="gauge-val-overlay">--</div>
        </div>
        <div class="aqi-details">
          <div class="aqi-level" id="aqi-level-text" style="color:var(--green)">--</div>
          <div class="aqi-threshold">Alert at <span id="thrText">150</span></div>
          <div class="threshold-row">
            <input type="range" id="thr" min="0" max="500" value="150">
            <button class="btn threshold-save" id="btnSet">Set</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="section-title"><span class="section-icon">📊</span> Historical Analytics</div>
  <div class="grid-2">
    <div class="card analytics-card">
      <div class="analytics-header">
        <div class="analytics-dot" style="background:var(--cyan);box-shadow:0 0 6px var(--cyan)"></div>
        <div class="analytics-title">Temperature History</div>
      </div>
      <div class="stat-row"><span class="stat-row-label">Maximum</span><span class="stat-row-val" id="t-max" style="color:var(--cyan)">-- °C</span></div>
      <div class="stat-row"><span class="stat-row-label">Average</span><span class="stat-row-val" id="t-avg" style="color:var(--cyan)">-- °C</span></div>
      <div class="stat-row"><span class="stat-row-label">Minimum</span><span class="stat-row-val" id="t-min" style="color:var(--cyan)">-- °C</span></div>
    </div>
    <div class="card analytics-card">
      <div class="analytics-header">
        <div class="analytics-dot" style="background:var(--pink);box-shadow:0 0 6px var(--pink)"></div>
        <div class="analytics-title">Humidity History</div>
      </div>
      <div class="stat-row"><span class="stat-row-label">Maximum</span><span class="stat-row-val" id="h-max" style="color:var(--pink)">-- %</span></div>
      <div class="stat-row"><span class="stat-row-label">Average</span><span class="stat-row-val" id="h-avg" style="color:var(--pink)">-- %</span></div>
      <div class="stat-row"><span class="stat-row-label">Minimum</span><span class="stat-row-val" id="h-min" style="color:var(--pink)">-- %</span></div>
    </div>
    <div class="card analytics-card">
      <div class="analytics-header">
        <div class="analytics-dot" style="background:var(--blue);box-shadow:0 0 6px var(--blue)"></div>
        <div class="analytics-title">Rain History</div>
      </div>
      <div class="stat-row"><span class="stat-row-label">Maximum</span><span class="stat-row-val" id="r-max" style="color:var(--blue)">-- %</span></div>
      <div class="stat-row"><span class="stat-row-label">Average</span><span class="stat-row-val" id="r-avg" style="color:var(--blue)">-- %</span></div>
      <div class="stat-row"><span class="stat-row-label">Minimum</span><span class="stat-row-val" id="r-min" style="color:var(--blue)">-- %</span></div>
    </div>
    <div class="card analytics-card">
      <div class="analytics-header">
        <div class="analytics-dot" style="background:var(--amber);box-shadow:0 0 6px var(--amber)"></div>
        <div class="analytics-title">AQI History</div>
      </div>
      <div class="stat-row"><span class="stat-row-label">Maximum</span><span class="stat-row-val" id="a-max" style="color:var(--amber)">--</span></div>
      <div class="stat-row"><span class="stat-row-label">Average</span><span class="stat-row-val" id="a-avg" style="color:var(--amber)">--</span></div>
      <div class="stat-row"><span class="stat-row-label">Minimum</span><span class="stat-row-val" id="a-min" style="color:var(--amber)">--</span></div>
    </div>
  </div>

  <div class="section-title"><span class="section-icon">📈</span> Live Trend Analysis</div>
  <div class="grid-charts">
    <div class="card chart-card">
      <div class="chart-header">
        <div class="chart-title">Temperature Trend</div>
        <div class="chart-current" style="color:var(--cyan)" id="ct-live">-- °C</div>
      </div>
      <div class="chart-wrap"><canvas id="chartTemp"></canvas></div>
    </div>
    <div class="card chart-card">
      <div class="chart-header">
        <div class="chart-title">Humidity Trend</div>
        <div class="chart-current" style="color:var(--pink)" id="ch-live">-- %</div>
      </div>
      <div class="chart-wrap"><canvas id="chartHum"></canvas></div>
    </div>
    <div class="card chart-card">
      <div class="chart-header">
        <div class="chart-title">Rain Intensity Trend</div>
        <div class="chart-current" style="color:var(--blue)" id="cr-live">-- %</div>
      </div>
      <div class="chart-wrap"><canvas id="chartRain"></canvas></div>
    </div>
    <div class="card chart-card">
      <div class="chart-header">
        <div class="chart-title">AQI Trend</div>
        <div class="chart-current" style="color:var(--amber)" id="ca-live">--</div>
      </div>
      <div class="chart-wrap"><canvas id="chartMq2"></canvas></div>
    </div>
  </div>

  <div class="section-title"><span class="section-icon">⚙️</span> System Diagnostics</div>
  <div class="card log-card">
    <div class="chart-header">
      <div class="chart-title">Live System Log</div>
      <div class="chart-current" style="color:var(--muted); font-size:11px;">Polling Sensors</div>
    </div>
    <div class="log-box" id="sysLog">
      <div class="log-entry"><span class="log-time">[Init]</span><span class="log-msg">SkyCast Interface Ready...</span></div>
    </div>
  </div>

  <footer class="footer">
    <div>SkyCast v2 · ESP32 Environmental Monitor</div>
    <div class="footer-right">
      <span>Logs every 60s</span>
      <span id="last-update">Last update: --</span>
    </div>
  </footer>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/gsap/3.12.2/gsap.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<script>
// ─── PARTICLE CANVAS ───
(function(){
  const c = document.getElementById('particle-canvas');
  const ctx = c.getContext('2d');
  let W, H, pts = [];
  function resize(){ W = c.width = innerWidth; H = c.height = innerHeight; }
  resize(); window.addEventListener('resize', resize);
  for(let i=0; i<80; i++) pts.push({
    x: Math.random()*1400, y: Math.random()*900,
    vx: (Math.random()-.5)*.25, vy: (Math.random()-.5)*.25,
    r: Math.random()*1.5+.5, a: Math.random()*.6+.1
  });
  function draw(){
    ctx.clearRect(0,0,W,H);
    pts.forEach(p=>{
      p.x=(p.x+p.vx+W)%W; p.y=(p.y+p.vy+H)%H;
      ctx.beginPath(); ctx.arc(p.x,p.y,p.r,0,Math.PI*2);
      ctx.fillStyle=`rgba(56,189,248,${p.a})`; ctx.fill();
    });
    for(let i=0;i<pts.length;i++) for(let j=i+1;j<pts.length;j++){
      const dx=pts[i].x-pts[j].x, dy=pts[i].y-pts[j].y, d=Math.sqrt(dx*dx+dy*dy);
      if(d<100){ ctx.beginPath(); ctx.moveTo(pts[i].x,pts[i].y); ctx.lineTo(pts[j].x,pts[j].y);
        ctx.strokeStyle=`rgba(56,189,248,${(1-d/100)*0.12})`; ctx.lineWidth=.5; ctx.stroke(); }
    }
    requestAnimationFrame(draw);
  }
  draw();
})();

// ─── CLOCK ───
function updateClock(){ document.getElementById('clock').textContent = new Date().toLocaleTimeString(); }
setInterval(updateClock, 1000); updateClock();

// ─── PAGE ENTRANCE ───
gsap.from('.header', { duration: .7, y: -20, opacity: 0, ease: 'power2.out' });
gsap.from('.section-title', { duration: .6, y: 15, opacity: 0, stagger: .15, delay: .1, ease: 'power2.out' });
gsap.from('.stat-card', { duration: .6, y: 30, opacity: 0, stagger: .12, delay: .2, ease: 'power2.out' });
gsap.from('.analytics-card', { duration: .6, y: 20, opacity: 0, stagger: .1, delay: .5, ease: 'power2.out' });
gsap.from('.chart-card', { duration: .6, y: 20, opacity: 0, stagger: .1, delay: .7, ease: 'power2.out' });
gsap.from('.log-card', { duration: .6, y: 20, opacity: 0, delay: .9, ease: 'power2.out' });

// ─── LOGGING ───
function addLog(msg, isAlert = false) {
  const logBox = document.getElementById('sysLog');
  if(logBox.children.length > 50) logBox.removeChild(logBox.firstChild);
  const time = new Date().toLocaleTimeString();
  const colorClass = isAlert ? 'log-alert' : 'log-msg';
  logBox.innerHTML += `<div class="log-entry"><span class="log-time">[${time}]</span><span class="${colorClass}">${msg}</span></div>`;
  logBox.scrollTop = logBox.scrollHeight; 
}

// ─── GAUGE ───
const gaugePath = document.getElementById('gauge-track');
const gaugeNeedle = document.getElementById('gauge-needle');
const gaugeOverlay = document.getElementById('gauge-val-overlay');
const totalArc = 157;

function updateGauge(aqi){
  const pct = Math.min(Math.max(aqi/500,0),1);
  const offset = totalArc - totalArc*pct;
  gaugePath.style.strokeDashoffset = offset;
  const angle = Math.PI * pct; 
  const cx = 60, cy = 60, r = 50;
  const nx = cx - r*Math.cos(angle);
  const ny = cy - r*Math.sin(angle);
  gaugeNeedle.setAttribute('cx', nx);
  gaugeNeedle.setAttribute('cy', ny);
  gaugeOverlay.textContent = aqi;
}

function getAQILevel(v){
  if(v<=50)  return ['Good','#34d399'];
  if(v<=100) return ['Moderate','#fbbf24'];
  if(v<=150) return ['Unhealthy for Sensitive Groups','#f97316'];
  if(v<=200) return ['Unhealthy','#f87171'];
  if(v<=300) return ['Very Unhealthy','#a855f7'];
  return ['Hazardous','#ef4444'];
}

// ─── CHARTS ───
const MAX_PTS = 60;
const hist = { ts:[], t:[], h:[], aqi:[], r:[] };

function makeGrad(ctx, hex){
  const g = ctx.createLinearGradient(0,0,0,140);
  g.addColorStop(0, hex+'33'); g.addColorStop(1, hex+'00'); return g;
}
function mkChart(id, color, label){
  const ctx = document.getElementById(id).getContext('2d');
  return new Chart(ctx, {
    type: 'line',
    data: { labels: hist.ts, datasets: [{
      label, data: [],
      borderColor: color, backgroundColor: makeGrad(ctx, color),
      borderWidth: 2, fill: true, tension: 0.4, pointRadius: 0,
      pointHoverRadius: 5, pointHoverBackgroundColor: color
    }]},
    options: {
      responsive: true, maintainAspectRatio: false,
      animation: { duration: 300 },
      plugins: { legend: { display: false }, tooltip: {
        backgroundColor: '#0d1525', titleColor: '#94a3b8',
        bodyColor: '#e8edf5', borderColor: 'rgba(99,179,237,0.2)',
        borderWidth: 1, padding: 10, cornerRadius: 8,
      }},
      scales: {
        x: { display: false },
        y: { grid: { color: 'rgba(255,255,255,0.04)', drawBorder: false },
             ticks: { color: '#5a7498', font: { size: 10, family: "'JetBrains Mono'" } },
             border: { display: false }
        }
      }
    }
  });
}
const cT = mkChart('chartTemp','#38bdf8','Temp');
const cH = mkChart('chartHum','#f472b6','Hum');
const cR = mkChart('chartRain','#3b82f6','Rain');
const cM = mkChart('chartMq2','#fbbf24','AQI');

// ─── ANIMATED NUMBER ───
function animateNum(el, newVal, decimals=1){
  const from = parseFloat(el.textContent) || 0;
  gsap.to({ v: from }, {
    v: newVal, duration: .8, ease: 'power2.out',
    onUpdate: function(){ el.textContent = this.targets()[0].v.toFixed(decimals); }
  });
}

// ─── SENSOR POLL ───
async function updateSensor(){
  try{
    const d = await (await fetch('/sensor')).json();
    const now = new Date().toLocaleTimeString();

    // Numbers
    animateNum(document.getElementById('temp'), d.temperature ?? 0, 1);
    animateNum(document.getElementById('hum'), d.humidity ?? 0, 1);
    animateNum(document.getElementById('rain'), d.rainPct ?? 0, 0);
    
    document.getElementById('ct-live').textContent = (d.temperature??'--') + ' °C';
    document.getElementById('ch-live').textContent = (d.humidity??'--') + ' %';
    document.getElementById('cr-live').textContent = (d.rainPct??'--') + ' %';
    document.getElementById('ca-live').textContent = d.aqi ?? '--';

    // Rain Badge Logic
    const rainBadge = document.getElementById('rain-badge');
    if(d.isRaining) {
       rainBadge.textContent = "RAINING";
       rainBadge.style.background = "var(--blue)";
       rainBadge.style.color = "#fff";
    } else {
       rainBadge.textContent = "DRY";
       rainBadge.style.background = "var(--blue-dim)";
       rainBadge.style.color = "var(--blue)";
    }

    // Gauge + level
    updateGauge(d.aqi ?? 0);
    const [lvl, col] = getAQILevel(d.aqi ?? 0);
    const aqiEl = document.getElementById('aqi-level-text');
    aqiEl.textContent = lvl; aqiEl.style.color = col;

    // Alerts
    const hasAlert = d.aqiAlert || d.isRaining;
    document.getElementById('alertBanner').classList.toggle('show', hasAlert);
    
    // Update System Log
    const logMsg = `T:${d.temperature}°C | H:${d.humidity}% | R:${d.rainPct}% | AQI:${d.aqi}`;
    addLog(logMsg, hasAlert);

    // Push to charts
    if(hist.ts.length >= MAX_PTS){
      hist.ts.shift(); 
      cT.data.datasets[0].data.shift();
      cH.data.datasets[0].data.shift(); 
      cR.data.datasets[0].data.shift();
      cM.data.datasets[0].data.shift();
    }
    hist.ts.push(now);
    cT.data.datasets[0].data.push(d.temperature);
    cH.data.datasets[0].data.push(d.humidity);
    cR.data.datasets[0].data.push(d.rainPct);
    cM.data.datasets[0].data.push(d.aqi);
    
    cT.update('none'); cH.update('none'); cR.update('none'); cM.update('none');

    document.getElementById('last-update').textContent = 'Last update: ' + now;
  } catch(e){
    addLog('Error fetching sensor data', true);
  }
}

// ─── STATS ───
async function loadStats(){
  try{
    const res = await fetch('/export?t=' + new Date().getTime());
    if(!res.ok) return;
    const text = await res.text();
    const lines = text.trim().split('\n');
    let tA=[], hA=[], aA=[], rA=[];
    
    for(let i=1;i<lines.length;i++){
      const c=lines[i].split(',');
      if(c.length >= 8){
        const t=parseFloat(c[2]), h=parseFloat(c[3]), a=parseInt(c[4]);
        let r = 0;
        if(c.length >= 8) { r = parseInt(c[5]); } 

        if(!isNaN(t)&&t!==0) tA.push(t);
        if(!isNaN(h)&&h!==0) hA.push(h);
        if(!isNaN(a)) aA.push(a);
        if(!isNaN(r)) rA.push(r);
      }
    }
    const calc=(arr)=>{
      if(!arr.length) return {max:'--',min:'--',avg:'--'};
      let s=0,mn=arr[0],mx=arr[0];
      for(let v of arr){s+=v;if(v<mn)mn=v;if(v>mx)mx=v;}
      return {max:mx.toFixed(1),min:mn.toFixed(1),avg:(s/arr.length).toFixed(1)};
    };
    
    const tS=calc(tA), hS=calc(hA), aS=calc(aA), rS=calc(rA);
    
    document.getElementById('t-max').textContent=tS.max+' °C';
    document.getElementById('t-avg').textContent=tS.avg+' °C';
    document.getElementById('t-min').textContent=tS.min+' °C';
    
    document.getElementById('h-max').textContent=hS.max+' %';
    document.getElementById('h-avg').textContent=hS.avg+' %';
    document.getElementById('h-min').textContent=hS.min+' %';
    
    document.getElementById('r-max').textContent=(rS.max==='--'?'--':Math.round(rS.max))+' %';
    document.getElementById('r-avg').textContent=(rS.avg==='--'?'--':Math.round(rS.avg))+' %';
    document.getElementById('r-min').textContent=(rS.min==='--'?'--':Math.round(rS.min))+' %';

    document.getElementById('a-max').textContent=Math.round(aS.max)||'--';
    document.getElementById('a-avg').textContent=Math.round(aS.avg)||'--';
    document.getElementById('a-min').textContent=Math.round(aS.min)||'--';
    
    gsap.from('.stat-row-val', { duration: .5, y: 8, opacity: 0, stagger: .05, ease: 'power2.out' });
  } catch(e){}
}

// ─── CONTROLS ───
const thr = document.getElementById('thr');
thr.addEventListener('input', ()=> document.getElementById('thrText').textContent = thr.value);
document.getElementById('btnSet').onclick = ()=>{
  fetch('/setthreshold?value='+thr.value);
  addLog(`AQI Threshold set to ${thr.value}`);
  gsap.fromTo('#btnSet', { scale: 0.85 }, { scale: 1, duration: 0.4, ease: 'back.out(2)' });
};
document.getElementById('btnExport').onclick = ()=> {
  addLog('Exporting CSV data...');
  window.location='/export?download=' + new Date().getTime();
}
document.getElementById('btnClear').onclick = async()=>{
  if(confirm('Delete all historical data?')){ 
    await fetch('/clear'); 
    addLog('Historical data cleared', true);
    loadStats(); 
  }
};
document.getElementById('btnRefreshStats').onclick = ()=>{
  gsap.fromTo('#btnRefreshStats', 
    { scale: 0.85 }, 
    { scale: 1, duration: 0.4, ease: 'back.out(2)' }
  );
  addLog('Refreshing historical stats...');
  loadStats();
};

// ─── IP ───
fetch('/meta').then(r=>r.text()).then(ip=>{
  document.getElementById('ip-addr').textContent = ip;
}).catch(()=> document.getElementById('ip-addr').textContent = 'offline');

// ─── INIT ───
setInterval(updateSensor, 2000);
updateSensor();
loadStats();
</script>
</body></html>
)rawliteral";
}

// --- Handlers ---
void handleRoot(){ server.send(200,"text/html",getDashboardPage()); }
void handleMeta(){ server.send(200,"text/plain",WiFi.localIP().toString()); }
void handleSetThreshold(){ 
  if(server.hasArg("value")) aqiThreshold = server.arg("value").toInt(); 
  server.send(200,"text/plain","OK"); 
}

void handleExport(){
  if(!SPIFFS.exists(csvPath)){ server.send(404,"text/plain","No Data"); return; }
  File f=SPIFFS.open(csvPath,"r"); 
  
  // Explicit headers to strictly prevent caching and force a file download
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Content-Disposition", "attachment; filename=\"skycast_data.csv\"");
  
  server.streamFile(f,"text/csv"); 
  f.close();
}

void handleClear(){ SPIFFS.remove(csvPath); ensureCsvHeader(); server.send(200,"text/plain","OK"); }

void handleSensor(){
  String json="{";
  json += "\"temperature\":" + (isnan(lastTemp) ? "null" : String(lastTemp,1)) + ",";
  json += "\"humidity\":" + (isnan(lastHum) ? "null" : String(lastHum,1)) + ",";
  json += "\"aqi\":" + String(lastAqi) + ",";
  json += "\"rainPct\":" + String(lastRainPct) + ",";
  json += "\"isRaining\":" + String(lastIsRaining ? "true" : "false") + ",";
  json += "\"aqiAlert\":" + String((lastAqi > aqiThreshold) ? "true" : "false");

  json += "}";
  server.send(200, "application/json", json);
}

int calculateAQI(int mqValue) {
  int aqi = map(mqValue, 0, 4095, 0, 500); // Convert sensor value to AQI scale
  if(aqi < 0) aqi = 0;
  if(aqi > 500) aqi = 500;
  return aqi;
}

void setup(){
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n\n--- BOOTING SKYCAST ---");

  // --- SENSOR INITIALIZATION ---
  Serial.println("Initializing Sensors...");

  dht.begin();
  Serial.println("DHT11 → OK");

  pinMode(MQ2_PIN, INPUT);
  Serial.println("MQ2 Gas Sensor → OK");

  pinMode(RAIN_DO_PIN, INPUT);
  pinMode(RAIN_AO_PIN, INPUT);   
  Serial.println("Rain Sensor → OK");

  pinMode(LED_PIN, OUTPUT);

  analogReadResolution(12);

  Serial.println("All Sensors Initialized Successfully");

  Serial.println("Step 1: Mounting Storage...");
  if(!SPIFFS.begin(true)){ 
    Serial.println("! SPIFFS Mount Failed. Formatting...");
  } else {
    Serial.println("-> Storage Mounted.");
    ensureCsvHeader();
  }

  Serial.println("Step 2: Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int retry = 0;

  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.print("\n-> WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n! WiFi Failed. Restarting...");
    ESP.restart();
  }

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase Connected ✅");

  Serial.println("Step 3: Syncing Time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  server.on("/", handleRoot);
  server.on("/meta", handleMeta);
  server.on("/sensor", handleSensor);
  server.on("/setthreshold", handleSetThreshold);
  server.on("/export", handleExport);
  server.on("/clear", handleClear);

  server.begin();
  Serial.println("-> Server Started. Ready.");
}

void loop(){
  server.handleClient();

  // WiFi reconnect attempt every 10 seconds if disconnected
  static unsigned long lastReconnectAttempt = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt > 10000) {
    lastReconnectAttempt = millis();
    WiFi.reconnect();
  }
  
  // 1. Read sensors frequently for the UI and LED
  if (millis() - lastSensorMillis >= sensorInterval) {
    lastSensorMillis = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Check if DHT failed
    if(isnan(h) || isnan(t)) {
      Serial.println("⚠ DHT read failed");
    } else {
      lastHum = h;
      lastTemp = t;
    }

    int sum = 0;
    for(int i = 0; i < 5; i++){
      sum += analogRead(MQ2_PIN);
    }

    lastMq2 = sum / 5;
    lastAqi = calculateAQI(lastMq2);



    // Rain Sensor Processing
    int rainRaw = analogRead(RAIN_AO_PIN);
    lastRainPct = map(rainRaw, 3500, 1000, 0, 100);
    lastRainPct = constrain(lastRainPct, 0, 100);
    
    // Digital Rain Alert with noise filtering
    static int rainCounter = 0;

    if(!digitalRead(RAIN_DO_PIN)) {   // Rain signal detected
      rainCounter++;
    } else {
      rainCounter = 0;
    }

    lastIsRaining = (rainCounter > 2);

    FirebaseJson json;

    json.set("temperature", lastTemp);
    json.set("humidity", lastHum);
    json.set("aqi", lastAqi);
    json.set("rain", lastRainPct);
    json.set("isRaining", lastIsRaining);

    if (Firebase.RTDB.setJSON(&fbdo, "/sensor", &json)) {
      Serial.println("Firebase OK ✅");
    } else {
      Serial.print("Firebase Error ❌: ");
      Serial.println(fbdo.errorReason()); 
    }

    delay(10);

    if(lastAqi > aqiThreshold || lastIsRaining)
      digitalWrite(LED_PIN, HIGH);
    else
      digitalWrite(LED_PIN, LOW);
  }

  // 2. Log to CSV much slower to protect Flash Memory (every 60 seconds)
  if (millis() - lastLogMillis >= logInterval) {
    lastLogMillis = millis();

    bool aqiAlert = (lastAqi > aqiThreshold);
    saveReadingToCSV(lastTemp, lastHum, lastAqi, lastRainPct, aqiAlert, lastIsRaining);
  }
}
