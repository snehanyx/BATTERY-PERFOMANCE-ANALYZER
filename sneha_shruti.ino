#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================= WIFI ================= */
const char* ssid = "iPhone";
const char* password = "887654321";
WiFiServer server(80);

/* ================= INA219 ================= */
Adafruit_INA219 ina219;

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ================= BATTERY PARAMETERS ================= */
float batteryCapacity_mAh = 2000.0;

/* ================= STATE VARIABLES ================= */
float voltage = 0;
float current_mA = 0;
float power_mW = 0;
float energy_Wh = 0;

float SOC = 100;
float SOH = 100;
float DoD = 0;
float cycleCount = 0;

unsigned long lastTime;

/* ============================================================= */

float voltageToSOC(float v) {
  if (v >= 4.20) return 100;
  if (v <= 3.20) return 0;
  return (v - 3.20) * 100.0 / (4.20 - 3.20);
}

/* ============================================================= */

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println("\n===== SYSTEM START =====");

  /* -------- INA219 -------- */
  if (!ina219.begin()) {
    Serial.println("INA219 not found!");
    while (1);
  } else {
    Serial.println("INA219 OK");
  }

  /* -------- OLED -------- */
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while(1);
  } else {
    Serial.println("OLED OK");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  /* -------- WIFI -------- */
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int timeout = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    timeout++;

    if (timeout > 20) {   // 10 seconds timeout
      Serial.println("\nWiFi FAILED!");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  server.begin();
  lastTime = millis();
}

/* ============================================================= */

void loop() {

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  /* ========= SENSOR DATA ========= */
  float busVoltage = ina219.getBusVoltage_V();
  float shuntVoltage = ina219.getShuntVoltage_mV() / 1000.0;

  voltage = busVoltage + shuntVoltage;
  current_mA = ina219.getCurrent_mA();
  power_mW = voltage * current_mA;

  /* ========= ENERGY ========= */
  energy_Wh += (power_mW / 1000.0) * (dt / 3600.0);

  /* ========= SOC ========= */
  SOC = constrain(voltageToSOC(voltage), 0, 100);
  DoD = 100 - SOC;

  /* ========= CYCLE COUNT ========= */
  cycleCount += abs(current_mA) * dt /
                (batteryCapacity_mAh * 3600.0 * 2);

  SOH = constrain(100 - (cycleCount * 0.03), 0, 100);

  /* ================= OLED ================= */
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("V: "); display.println(voltage,2);
  display.print("I: "); display.println(current_mA,1);
  display.print("SOC: "); display.print(SOC,1); display.println("%");
  display.print("SOH: "); display.print(SOH,1); display.println("%");
  display.display();

  /* ================= WEB ================= */
 WiFiClient client = server.available();
if (!client) return;

String request = client.readStringUntil('\r');
client.flush();

/* ===== SEND JSON DATA ===== */
if (request.indexOf("/data") != -1) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{");
  client.printf("\"voltage\":%.2f,", voltage);
  client.printf("\"current\":%.2f,", current_mA);
  client.printf("\"power\":%.2f,", power_mW);
  client.printf("\"energy\":%.4f,", energy_Wh);
  client.printf("\"soc\":%.2f,", SOC);
  client.printf("\"soh\":%.2f,", SOH);
  client.printf("\"dod\":%.2f,", DoD);
  client.printf("\"cycles\":%.3f,", cycleCount);
  client.printf("\"temp\":%.2f", 30.0); // change if temp sensor available
  client.print("}");

  client.stop();
  return;
}

/* ===== SEND DASHBOARD HTML ===== */
client.println("HTTP/1.1 200 OK");
client.println("Content-Type:text/html");
client.println("Connection: close");
client.println();

/* IMPORTANT: PASTE YOUR FULL TESLA UI HTML HERE */
client.println(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EV Battery Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>

body{
margin:0;
font-family:'Segoe UI';
background:radial-gradient(circle at top,#0f2027,#203a43,#2c5364);
color:#fff;
}

/* HEADER */
.header{
display:flex;
justify-content:space-between;
padding:15px 25px;
background:rgba(0,0,0,0.4);
backdrop-filter:blur(10px);
}

.title{
font-size:20px;
font-weight:bold;
color:#00ffcc;
}

.status{
font-size:14px;
color:#aaa;
}

/* GRID */
.dashboard{
display:grid;
grid-template-columns:repeat(auto-fit,minmax(250px,1fr));
gap:20px;
padding:20px;
}

/* CARD */
.card{
background:rgba(255,255,255,0.05);
border-radius:18px;
padding:18px;
box-shadow:0 10px 30px rgba(0,0,0,0.6);
backdrop-filter:blur(12px);
transition:0.3s;
}

.card:hover{
transform:translateY(-6px);
box-shadow:0 0 25px rgba(0,255,150,0.5);
}

.important{
border:1px solid rgba(0,255,150,0.4);
}

.highlight{
color:#00ffcc;
font-size:22px;
font-weight:bold;
}

/* BATTERY */
.battery{
width:100px;height:45px;
border:2px solid #00ffcc;
margin:auto;
position:relative;
border-radius:6px;
}
.battery::after{
content:'';
position:absolute;
right:-8px;
top:12px;
width:6px;height:20px;
background:#00ffcc;
}
.battery-level{
height:100%;
background:linear-gradient(90deg,#00ffcc,#00ffaa);
border-radius:4px;
}
.charging{animation:pulse 1s infinite;}
@keyframes pulse{0%{opacity:0.5;}100%{opacity:1;}}

/* GAUGE */
.gauge{
width:150px;height:150px;
border-radius:50%;
display:flex;
align-items:center;
justify-content:center;
margin:auto;
font-size:22px;
font-weight:bold;
}

/* ALERT COLORS */
.alert-green{color:#00ff88;}
.alert-yellow{color:#ffcc00;}
.alert-red{color:#ff4444;}

/* AI PANEL */
.ai-box{
background:rgba(0,255,150,0.08);
padding:10px;
border-radius:10px;
margin-top:10px;
}

/* GRAPH */
canvas{
height:260px !important;
}

</style>
</head>

<body>

<div class="header">
<div class="title"> EV Smart Dashboard</div>
<div class="status">Live System  ESP32</div>
</div>

<div class="dashboard">

<!-- BATTERY -->
<div class="card important">
<h3>Battery Status</h3>
<div class="battery"><div id="batteryLevel" class="battery-level"></div></div>
</div>

<!-- SOC -->
<div class="card important">
<h3>SOC</h3>
<div class="gauge"><span id="soc">--</span></div>
</div>

<!-- BASIC DATA -->
<div class="card">Voltage <span id="voltage" class="highlight"></span></div>
<div class="card">Current <span id="current" class="highlight"></span></div>
<div class="card">Power <span id="power" class="highlight"></span></div>
<div class="card">Temp <span id="temp" class="highlight"></span></div>

<!-- HEALTH -->
<div class="card">SOH <span id="soh" class="highlight"></span></div>
<div class="card">DoD <span id="dod" class="highlight"></span></div>
<div class="card">Cycles <span id="cycles" class="highlight"></span></div>

<!-- ALERT -->
<div class="card important">
<h3>System Alerts</h3>
<p id="alerts" class="alert-green">Normal</p>
</div>

<!-- AI -->
<div class="card">
<h3>🤖 AI Analysis</h3>
<p id="ai"></p>
<div class="ai-box" id="insights">Analyzing...</div>
</div>

<!-- TIME -->
<div class="card important">
<h3>Remaining Time</h3>
<p id="time" class="highlight"></p>
</div>

<!-- SCORE -->
<div class="card">
<h3>Battery Score</h3>
<p id="score" class="highlight"></p>
</div>

<!-- GRAPH -->
<div class="card">
<h3>Live Multi-Graph</h3>
<canvas id="multiChart"></canvas>
</div>

</div>
<script>
// ================= FETCH LIVE DATA =================
setInterval(async () => {
  try {
    const res = await fetch('/data');
    const data = await res.json();

    updateData(
      data.voltage,
      data.current,
      data.power,
      data.energy,
      data.soc,
      data.soh,
      data.dod,
      data.cycles,
      data.temp
    );

  } catch (err) {
    console.log("Fetch Error:", err);
  }
}, 1000); // update every 1 sec
// ================= GRAPH =================
let labels=[], voltageData=[], currentData=[], powerData=[];

const chart=new Chart(document.getElementById('multiChart'),{
type:'line',
data:{
labels:labels,
datasets:[
{label:'Voltage',data:voltageData,borderColor:'#00ffcc',tension:0.4,yAxisID:'y1'},
{label:'Current',data:currentData,borderColor:'#00aaff',tension:0.4,yAxisID:'y2'},
{label:'Power',data:powerData,borderColor:'#ffaa00',tension:0.4,yAxisID:'y1'}
]},
options:{
animation:{duration:600},
scales:{
y1:{position:'left'},
y2:{position:'right'}
}}
});

// ================= MAIN UPDATE FUNCTION =================
function updateData(v,c,p,e,soc,soh,dod,cycles,temp){

// BASIC UI
// ================= FORMATTED VALUES WITH UNITS =================
document.getElementById('voltage').innerText = v.toFixed(2) + " V";

document.getElementById('current').innerText =
  (Math.abs(c) > 1000)
    ? (c/1000).toFixed(2) + " A"
    : c.toFixed(0) + " mA";

document.getElementById('power').innerText =
  (p > 1000)
    ? (p/1000).toFixed(2) + " W"
    : p.toFixed(0) + " mW";

document.getElementById('temp').innerText = temp.toFixed(1) + " °C";
document.getElementById('soc').innerText=soc.toFixed(1)+"%";
document.getElementById('soh').innerText = soh.toFixed(1) + " %";
document.getElementById('dod').innerText = dod.toFixed(1) + " %";
document.getElementById('cycles').innerText = cycles.toFixed(1) + " cycles";

// BATTERY
let b=document.getElementById('batteryLevel');
b.style.width=soc+"%";
(c>0)?b.classList.add("charging"):b.classList.remove("charging");

// GAUGE
document.querySelector('.gauge').style.background=
`conic-gradient(#00ffcc 0% ${soc}%,#222 ${soc}% 100%)`;

// ALERTS
let alertBox=document.getElementById('alerts');
if(soc<20||temp>50){
alertBox.innerText="🔴 Critical";
alertBox.className="alert-red";
}
else if(soc<40){
alertBox.innerText="🟡 Warning";
alertBox.className="alert-yellow";
}
else{
alertBox.innerText="🟢 Normal";
alertBox.className="alert-green";
}

// ================= AI ANALYSIS =================
// ================= ADVANCED AI ANALYSIS =================
let ai = "Healthy";

// MULTI-CONDITION AI
if (soc < 20) {
  ai = "Low Battery";
}
else if (temp > 45) {
  ai = "Overheating";
}
else if (cycles > 300) {
  ai = "Battery Aging";
}
else if (dod > 70) {
  ai = "Deep Discharge Stress";
}
else if (Math.abs(c) > 2000) {
  ai = "High Current Load";
}
else {
  ai = "Optimal Condition";
}

document.getElementById('ai').innerText = ai;

// ================= REMAINING TIME =================
let time=(soc/100)*6;
document.getElementById('time').innerText=time.toFixed(1)+" hrs";

// ================= BATTERY SCORE =================
let score=100-(cycles*0.05)-(dod*0.2);
document.getElementById('score').innerText=Math.max(0,score.toFixed(0));

// ================= GRAPH =================
if(labels.length>100){
labels.shift(); voltageData.shift(); currentData.shift(); powerData.shift();
}

labels.push(new Date().toLocaleTimeString());
voltageData.push(v);
currentData.push(c/1000);
powerData.push(p/1000);

chart.update();

// ================= 🔥 AI INSIGHTS (YOUR REQUIRED PART) =================
let healthStatus = soh > 90 ? "Excellent" : soh > 70 ? "Good" : "Degrading";
let efficiency = (dod < 50 && temp < 40) ? "Optimal" : "Needs Attention";

document.getElementById('insights').innerHTML =
`🧠 <b>AI Insights</b><br><br>
Health: <span class="highlight">${soh.toFixed(1)}%</span> (${healthStatus})<br>
Cycles Left: <span class="highlight">${Math.max(0,(1000-cycles).toFixed(0))}</span><br>
Efficiency: <span class="highlight">${efficiency}</span>`;
}
let aiBox = document.getElementById('ai');

if (ai.includes("Overheating") || ai.includes("Low")) {
  aiBox.style.color = "#ff4444"; // red
}
else if (ai.includes("Aging") || ai.includes("Stress")) {
  aiBox.style.color = "#ffcc00"; // yellow
}
else {
  aiBox.style.color = "#00ff88"; // green
}
</script>
</body>
</html>
)rawliteral");

client.stop();
}