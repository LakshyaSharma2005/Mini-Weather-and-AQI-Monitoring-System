#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include <DHT.h>
#include <time.h>
#include "secrets.h"

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ2_PIN 34
#define LED_PIN 2

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

// --- TIME CONFIG ---
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";

WebServer server(80);
int gasThreshold = 700;
unsigned long lastSensorMillis = 0;
const unsigned long sensorInterval = 2000;

float lastTemp = NAN;
float lastHum  = NAN;
int   lastMq2  = 0;
const char* csvPath = "/data.csv";

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
    f.println("Date,Time,Temperature,Humidity,MQ2,Alert");
    f.close();
    Serial.println("-> CSV Header created.");
  }
}

void saveReadingToCSV(float t, float h, int m, bool alert) {
  File f = SPIFFS.open(csvPath, FILE_APPEND);
  if (!f) return;
  String line = getCurrentDate() + "," + getCurrentTime() + "," + 
                (isnan(t)?"0":String(t,1)) + "," + 
                (isnan(h)?"0":String(h,1)) + "," + 
                String(m) + "," + (alert?"1":"0");
  f.println(line);
  f.close();
}

// --- Dashboard (Mobile Responsive + Gradient Charts) ---
String getDashboardPage() {
  return R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>IoT Project</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
<style>
:root{ --bg:#0f172a; --card:#1e293b; --text:#f1f5f9; --muted:#94a3b8; --accent:#10b981; --cyan:#06b6d4; --pink:#ec4899; --orange:#f59e0b; --danger:#ef4444; }
*{box-sizing:border-box;font-family:'Inter',sans-serif}
body{margin:0;background:var(--bg);color:var(--text);padding-bottom:40px}
.wrapper{max-width:1100px;margin:0 auto;padding:20px}
.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:24px;flex-wrap:wrap;gap:15px}
.title h1{margin:0;font-size:22px;background:linear-gradient(90deg,var(--accent),var(--cyan));-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.title .sub{color:var(--muted);font-size:13px;margin-top:4px}
.controls{display:flex;gap:10px}
.btn{background:var(--card);border:1px solid rgba(255,255,255,0.1);color:var(--text);padding:8px 16px;border-radius:8px;font-size:13px;font-weight:600;cursor:pointer}
.btn.primary{background:var(--accent);color:#000;border:none}
.btn.danger{color:var(--danger);border-color:rgba(239,68,68,0.3)}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:16px}
.stat-card{grid-column:span 4;background:var(--card);border-radius:16px;padding:20px;border:1px solid rgba(255,255,255,0.05);display:flex;flex-direction:column;justify-content:space-between;height:140px}
.stat-label{color:var(--muted);font-size:13px;font-weight:600;text-transform:uppercase}
.stat-val{font-size:32px;font-weight:700;margin:10px 0}
.big-card{grid-column:span 12;height:300px;position:relative;background:var(--card);border-radius:16px;padding:20px;border:1px solid rgba(255,255,255,0.05)}
.chart-container{position:absolute;top:50px;left:15px;right:15px;bottom:15px}
.log-card{grid-column:span 12;height:200px;display:flex;flex-direction:column;background:var(--card);border-radius:16px;padding:20px;border:1px solid rgba(255,255,255,0.05)}
.log-box{flex:1;overflow-y:auto;font-family:monospace;font-size:12px;color:var(--muted);background:rgba(0,0,0,0.2);padding:10px;border-radius:8px;margin-top:10px}
@media (max-width:768px){ .stat-card{grid-column:span 12;height:auto} .header{flex-direction:column;align-items:flex-start} .controls{width:100%;justify-content:space-between} }
input[type=range]{width:100%;height:6px;background:#334155;border-radius:4px;outline:none;-webkit-appearance:none;margin-top:10px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;background:var(--accent);border-radius:50%;cursor:pointer}
</style>
</head>
<body>
<div class="wrapper">
  <div class="header">
    <div class="title"><h1>Mini Weather & AQI Monitoring System</h1><div class="sub">IoT Environment Station • <span id="ip">...</span></div></div>
    <div class="controls"><button class="btn primary" id="btnExport">Export CSV</button><button class="btn danger" id="btnClear">Clear Log</button></div>
  </div>
  <div class="grid">
    <div class="stat-card" style="border-top:4px solid var(--cyan)"><div class="stat-label">Temperature</div><div class="stat-val" style="color:var(--cyan)"><span id="temp">--</span> °C</div></div>
    <div class="stat-card" style="border-top:4px solid var(--pink)"><div class="stat-label">Humidity</div><div class="stat-val" style="color:var(--pink)"><span id="hum">--</span> %</div></div>
    <div class="stat-card" style="border-top:4px solid var(--orange)">
      <div class="stat-label">Air Quality</div><div class="stat-val" style="color:var(--orange)"><span id="mq2">--</span></div>
      <div><input type="range" id="thr" min="200" max="3000" value="700"><div style="font-size:11px;color:var(--muted);margin-top:5px">Threshold: <span id="thrText">700</span> <button id="btnSet" class="btn" style="padding:2px 6px;font-size:10px;margin-left:5px">SET</button></div></div>
    </div>
    <div class="big-card"><div class="stat-label">Temperature Trend</div><div class="chart-container"><canvas id="chartTemp"></canvas></div></div>
    <div class="big-card"><div class="stat-label">Humidity Trend</div><div class="chart-container"><canvas id="chartHum"></canvas></div></div>
    <div class="big-card"><div class="stat-label">Gas Level Trend</div><div class="chart-container"><canvas id="chartMq2"></canvas></div></div>
    <div class="log-card"><div class="stat-label">System Log</div><div class="log-box" id="log">Ready...</div></div>
  </div>
</div>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
const MAX_PTS=60; let history={ts:[],t:[],h:[],m:[]};
const r=document.getElementById('thr'); r.addEventListener('input',()=>document.getElementById('thrText').innerText=r.value);
document.getElementById('btnSet').onclick=()=>fetch('/setthreshold?value='+r.value);
document.getElementById('btnExport').onclick=()=>window.location='/export';
document.getElementById('btnClear').onclick=async()=>{if(confirm("Delete CSV?")){await fetch('/clear');document.getElementById('log').innerHTML="<div>Cleared</div>";}};
function log(m){const b=document.getElementById('log');b.innerHTML=`<div>[${new Date().toLocaleTimeString()}] ${m}</div>`+b.innerHTML;}
function grad(ctx,c){const g=ctx.createLinearGradient(0,0,0,300);g.addColorStop(0,c.replace(')',',0.5)').replace('rgb','rgba'));g.addColorStop(1,c.replace(')',',0)').replace('rgb','rgba'));return g;}
function mkChart(id,c,l){const ctx=document.getElementById(id).getContext('2d');return new Chart(ctx,{type:'line',data:{labels:history.ts,datasets:[{label:l,data:[],borderColor:c,backgroundColor:grad(ctx,c),fill:true,tension:0.4,pointRadius:0}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:false}},scales:{x:{display:false},y:{grid:{color:'rgba(255,255,255,0.05)'},ticks:{color:'#94a3b8'}}}}});}
const cT=mkChart('chartTemp','rgb(6,182,212)','Temp'), cH=mkChart('chartHum','rgb(236,72,153)','Hum'), cM=mkChart('chartMq2','rgb(245,158,11)','Gas');
async function up(){
  try{
    const d=await(await fetch('/sensor')).json(); const now=new Date().toLocaleTimeString();
    document.getElementById('temp').innerText=d.temperature??'--'; document.getElementById('hum').innerText=d.humidity??'--'; document.getElementById('mq2').innerText=d.mq2;
    if(history.ts.length>MAX_PTS){history.ts.shift();cT.data.datasets[0].data.shift();cH.data.datasets[0].data.shift();cM.data.datasets[0].data.shift();}
    history.ts.push(now); cT.data.datasets[0].data.push(d.temperature); cH.data.datasets[0].data.push(d.humidity); cM.data.datasets[0].data.push(d.mq2);
    cT.update('none');cH.update('none');cM.update('none'); log(`T:${d.temperature} H:${d.humidity} G:${d.mq2}`);
  }catch(e){}
}
setInterval(up,2000); fetch('/meta').then(r=>r.text()).then(ip=>document.getElementById('ip').innerText=ip);
</script></body></html>
)rawliteral";
}

// --- Handlers ---
void handleRoot(){ server.send(200,"text/html",getDashboardPage()); }
void handleMeta(){ server.send(200,"text/plain",WiFi.localIP().toString()); }
void handleSetThreshold(){ if(server.hasArg("value")) gasThreshold=server.arg("value").toInt(); server.send(200,"text/plain","OK"); }
void handleExport(){
  if(!SPIFFS.exists(csvPath)){ server.send(404,"text/plain","No Data"); return; }
  File f=SPIFFS.open(csvPath,"r"); server.streamFile(f,"text/csv"); f.close();
}
void handleClear(){ SPIFFS.remove(csvPath); ensureCsvHeader(); server.send(200,"text/plain","OK"); }
void handleSensor(){
  float h=dht.readHumidity(); float t=dht.readTemperature(); int mq=analogRead(MQ2_PIN);
  if(!isnan(t))lastTemp=t; if(!isnan(h))lastHum=h; lastMq2=mq;
  bool alert=(mq>gasThreshold);
  saveReadingToCSV(lastTemp,lastHum,lastMq2,alert);
  String json="{";
  json+="\"temperature\":"+(isnan(lastTemp)?"null":String(lastTemp,1))+",";
  json+="\"humidity\":"+(isnan(lastHum)?"null":String(lastHum,1))+",";
  json+="\"mq2\":"+String(lastMq2)+",";
  json+="\"alert\":"+String(alert?"true":"false")+"}";
  server.send(200,"application/json",json);
}

void setup(){
  // 1. Slow Startup to prevent brownouts
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n\n--- BOOTING SKYCAST ---");

  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);

  // 2. Safely Mount SPIFFS
  Serial.println("Step 1: Mounting Storage...");
  if(!SPIFFS.begin(true)){ 
    Serial.println("! SPIFFS Mount Failed. Formatting...");
    // If it fails, we just continue without logging to prevent crash loop
  } else {
    Serial.println("-> Storage Mounted.");
    ensureCsvHeader();
  }

  // 3. Connect WiFi
  Serial.println("Step 2: Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print("."); retry++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.print("\n-> WiFi Connected! IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n! WiFi Failed. Restarting...");
    ESP.restart();
  }

  // 4. Sync Time
  Serial.println("Step 3: Syncing Time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  // 5. Start Server
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
  if (millis() - lastSensorMillis >= sensorInterval) {
    lastSensorMillis = millis();
    if (analogRead(MQ2_PIN) > gasThreshold) digitalWrite(LED_PIN, HIGH); else digitalWrite(LED_PIN, LOW);
  }
}