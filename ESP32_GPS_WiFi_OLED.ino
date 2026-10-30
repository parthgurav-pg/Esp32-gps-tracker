#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>

// ESP32 DevKit / NEO-6M / 128x64 SSD1306 I2C OLED
constexpr int GPS_RX_PIN = 16;  // GPS TX -> ESP32 GPIO16
constexpr int GPS_TX_PIN = 17;  // Not connected to GPS in this project
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
constexpr uint8_t OLED_ADDR = 0x3C; // Try 0x3D if your OLED is blank
const char* AP_SSID = "ESP32-GPS-Tracker";
const char* AP_PASSWORD = "gps12345"; // Change for privacy (8+ chars)

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
WebServer server(80);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledReady = false;
uint32_t lastOLED = 0;
uint32_t lastGPSByte = 0;
uint32_t totalGPSBytes = 0;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 GPS Tracker</title>
<style>*{box-sizing:border-box}body{margin:0;background:#0d1626;color:#eaf2ff;font-family:system-ui,Arial}main{max-width:900px;margin:auto;padding:16px}h2{margin:0 0 4px}.muted{color:#a4b6cf;font-size:13px}.status{background:#20334c;border-radius:12px;padding:12px;margin:14px 0}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.metric{background:#1b2b43;border-radius:12px;padding:12px;min-width:0}.metric small{display:block;color:#a4b6cf}.metric b{display:block;margin-top:5px;font-size:18px;overflow-wrap:anywhere}.mapbox{position:relative;height:360px;margin-top:14px;border:1px solid #344861;border-radius:14px;overflow:hidden;background:#17283b;isolation:isolate}#offline{display:block;width:100%;height:100%}#online{position:absolute;inset:0;display:none;overflow:hidden}#online .leaflet-container{width:100%;height:100%}.maplabel{position:absolute;bottom:10px;left:10px;z-index:1000;background:#122238d9;padding:7px 10px;border-radius:8px;font-size:12px;pointer-events:none}button,a.btn{border:0;background:#327ddd;color:white;padding:11px 13px;border-radius:9px;display:inline-block;text-decoration:none;margin:10px 7px 0 0;font-size:14px}button:disabled{opacity:.5}.note{font-size:13px;color:#a4b6cf;line-height:1.5}@media(max-width:450px){.mapbox{height:320px}.metric b{font-size:16px}}</style>
</head><body><main><h2>🛰 ESP32 Live GPS Tracker</h2><div class="muted">Local Wi-Fi • GPS data refreshes every second</div><div class="status" id="status">Connecting to ESP32...</div>
<div class="grid"><div class="metric"><small>Latitude</small><b id="lat">--</b></div><div class="metric"><small>Longitude</small><b id="lng">--</b></div><div class="metric"><small>Satellites</small><b id="sat">--</b></div><div class="metric"><small>GPS fix</small><b id="fix">--</b></div><div class="metric"><small>Speed</small><b id="speed">--</b></div><div class="metric"><small>Altitude</small><b id="alt">--</b></div><div class="metric"><small>HDOP</small><b id="hdop">--</b></div><div class="metric"><small>Wi-Fi clients</small><b id="clients">--</b></div></div>
<div class="mapbox"><canvas id="offline"></canvas><div id="online"></div><div class="maplabel" id="maplabel">Offline GPS plot • Waiting for fix</div></div>
<button onclick="toggleFollow()" id="follow">Follow: ON</button><button onclick="clearTrail()">Clear trail</button><button onclick="loadOnlineMap()" id="mapbtn">Try online street map</button><a class="btn" id="mapsLink" href="#" target="_blank" rel="noopener" style="display:none">Open in Google Maps</a>
<p class="note">The offline GPS plot always works without internet; it shows relative movement, not roads. Online street tiles require phone internet access. If the phone is connected to the ESP32 hotspot with no internet, the online map may stay unavailable. GPS HDOP is not an accuracy value in metres.</p></main>
<script>
const E=id=>document.getElementById(id);let follow=true,points=[],latest=null,origin=null,onlineMap=null,onlineMarker=null,onlineTrail=null,onlineTiles=null,onlineReady=false;
function toggleFollow(){follow=!follow;E('follow').textContent='Follow: '+(follow?'ON':'OFF')}
function clearTrail(){points=[];if(latest)points=[latest];if(onlineTrail)onlineTrail.setLatLngs(points);drawOffline()}
function sizeCanvas(){let c=E('offline'),r=c.getBoundingClientRect(),d=Math.min(devicePixelRatio||1,2);c.width=Math.max(1,Math.round(r.width*d));c.height=Math.max(1,Math.round(r.height*d));drawOffline();if(onlineMap)onlineMap.invalidateSize()}
function drawOffline(){let c=E('offline'),x=c.getContext('2d'),w=c.width,h=c.height;x.clearRect(0,0,w,h);x.fillStyle='#17283b';x.fillRect(0,0,w,h);let step=Math.max(32,w/9);x.strokeStyle='#2a4059';x.lineWidth=1;for(let i=0;i<w;i+=step){x.beginPath();x.moveTo(i,0);x.lineTo(i,h);x.stroke()}for(let j=0;j<h;j+=step){x.beginPath();x.moveTo(0,j);x.lineTo(w,j);x.stroke()}if(!latest){x.fillStyle='#c4d2e4';x.font=`${Math.max(12,w/35)}px sans-serif`;x.fillText('Waiting for GPS coordinates',16,32);return}let center=follow?latest:(origin||latest);let metresPerPixel=2;let convert=p=>{let lat0=center[0]*Math.PI/180;return [w/2+(p[1]-center[1])*111320*Math.cos(lat0)/metresPerPixel,h/2-(p[0]-center[0])*111320/metresPerPixel]};if(points.length){x.beginPath();points.forEach((p,i)=>{let q=convert(p);if(i===0)x.moveTo(...q);else x.lineTo(...q)});x.strokeStyle='#60a5fa';x.lineWidth=3;x.stroke()}let q=convert(latest);x.beginPath();x.arc(q[0],q[1],11,0,Math.PI*2);x.fillStyle='#3187ff55';x.fill();x.beginPath();x.arc(q[0],q[1],6,0,Math.PI*2);x.fillStyle='#3b91ff';x.fill();x.strokeStyle='#fff';x.lineWidth=2;x.stroke();x.fillStyle='#bcd0e4';x.font=`${Math.max(11,w/42)}px sans-serif`;x.fillText('Offline relative position • ~2 m/pixel',12,h-16)}
function addPosition(p){latest=p;if(!origin)origin=p;if(!points.length||Math.abs(points[points.length-1][0]-p[0])>0.000002||Math.abs(points[points.length-1][1]-p[1])>0.000002){points.push(p);if(points.length>500)points.shift()}drawOffline();if(onlineMap){if(!onlineMarker){onlineMarker=L.circleMarker(p,{radius:9,color:'#fff',weight:2,fillColor:'#278bff',fillOpacity:1}).addTo(onlineMap)}else onlineMarker.setLatLng(p);onlineTrail.setLatLngs(points);if(follow)onlineMap.setView(p,Math.max(onlineMap.getZoom(),16));}}
function scriptLoad(url){return new Promise((resolve,reject)=>{let s=document.createElement('script');s.src=url;s.onload=resolve;s.onerror=reject;document.head.appendChild(s)})}
function styleLoad(url){return new Promise((resolve,reject)=>{let l=document.createElement('link');l.rel='stylesheet';l.href=url;l.onload=resolve;l.onerror=reject;document.head.appendChild(l)})}
async function loadOnlineMap(){E('mapbtn').disabled=true;E('maplabel').textContent='Loading online map resources...';try{await styleLoad('https://unpkg.com/leaflet@1.9.4/dist/leaflet.css');await scriptLoad('https://unpkg.com/leaflet@1.9.4/dist/leaflet.js');E('online').style.display='block';onlineMap=L.map('online',{zoomControl:true}).setView(latest||[20.5937,78.9629],latest?16:5);onlineTiles=L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; OpenStreetMap contributors'}).addTo(onlineMap);onlineTrail=L.polyline(points,{color:'#3187ff',weight:4}).addTo(onlineMap);if(latest)addPosition(latest);setTimeout(()=>onlineMap.invalidateSize(),150);E('maplabel').textContent='Online map • Tiles require internet';onlineReady=true}catch(err){E('online').style.display='none';E('maplabel').textContent='Offline GPS plot • Online map unavailable';E('mapbtn').disabled=false;E('mapbtn').textContent='Retry online map'}}
async function update(){try{let r=await fetch('/gps',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);let g=await r.json();E('sat').textContent=g.satellites??'--';E('hdop').textContent=g.hdop??'--';E('clients').textContent=g.clients??'--';E('fix').textContent=g.valid?'FIX OK':'SEARCHING';if(g.valid){E('lat').textContent=g.lat.toFixed(6);E('lng').textContent=g.lng.toFixed(6);E('speed').textContent=g.speed==null?'--':g.speed.toFixed(1)+' km/h';E('alt').textContent=g.altitude==null?'--':g.altitude.toFixed(1)+' m';E('status').textContent='GPS FIX OK • Live coordinates • Wi-Fi connected';addPosition([g.lat,g.lng]);E('mapsLink').href='https://www.google.com/maps?q='+g.lat+','+g.lng;E('mapsLink').style.display='inline-block';if(!onlineReady)E('maplabel').textContent='Offline GPS plot • Live position'}else{E('status').textContent=g.receiving?'Receiving GPS data • Waiting for fix':'No GPS serial data • Check TX to GPIO16'}}catch(e){E('status').textContent='ESP32 connection lost: '+e.message}}
window.addEventListener('resize',sizeCanvas);sizeCanvas();update();setInterval(update,1000);
</script></body></html>
)HTML";

bool hasFix() {
  return gps.location.isValid() && gps.location.age() < 3000;
}

void sendGPS() {
  bool fix = hasFix();
  bool receiving = totalGPSBytes > 0 && (millis() - lastGPSByte < 4000);
  String json = "{";
  json += "\"valid\":" + String(fix ? "true" : "false");
  json += ",\"receiving\":" + String(receiving ? "true" : "false");
  json += ",\"lat\":" + String(fix ? String(gps.location.lat(), 7) : "null");
  json += ",\"lng\":" + String(fix ? String(gps.location.lng(), 7) : "null");
  json += ",\"satellites\":" + String(gps.satellites.isValid() && gps.satellites.age() < 5000 ? String(gps.satellites.value()) : "null");
  json += ",\"speed\":" + String(fix && gps.speed.isValid() && gps.speed.age() < 5000 ? String(gps.speed.kmph(), 2) : "null");
  json += ",\"altitude\":" + String(fix && gps.altitude.isValid() && gps.altitude.age() < 5000 ? String(gps.altitude.meters(), 2) : "null");
  json += ",\"hdop\":" + String(gps.hdop.isValid() && gps.hdop.age() < 5000 ? String(gps.hdop.hdop(), 2) : "null");
  json += ",\"clients\":" + String(WiFi.softAPgetStationNum());
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void drawOLED() {
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 GPS TRACKER");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.print("GPS: ");
  if (hasFix()) display.println("FIX OK");
  else if (totalGPSBytes && millis() - lastGPSByte < 4000) display.println("SEARCHING");
  else display.println("NO DATA");
  display.print("Satellites: ");
  if (gps.satellites.isValid() && gps.satellites.age() < 5000) display.println(gps.satellites.value());
  else display.println("--");
  display.println("WiFi: ON");
  display.print("Phone: ");
  display.println(WiFi.softAPgetStationNum() > 0 ? "CONNECTED" : "NOT CONNECTED");
  display.println("IP: 192.168.4.1");
  display.display();
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Wire.begin(OLED_SDA, OLED_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledReady) Serial.println("OLED not found; check I2C wiring/address");
  WiFi.mode(WIFI_AP);
  bool apOK = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("WiFi AP: %s\nSSID: %s\nPassword: %s\nIP: %s\n", apOK ? "OK" : "FAILED", AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", PAGE); });
  server.on("/gps", HTTP_GET, sendGPS);
  server.onNotFound([](){server.send(404, "text/plain", "Not found");});
  server.begin();
  drawOLED();
}

void loop() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
    lastGPSByte = millis();
    totalGPSBytes++;
  }
  server.handleClient();
  if (millis() - lastOLED >= 1000) {
    lastOLED = millis();
    drawOLED();
    Serial.printf("GPS bytes=%lu fix=%d sats=%lu WiFi clients=%d\n",
                  (unsigned long)totalGPSBytes, hasFix(),
                  gps.satellites.isValid() ? (unsigned long)gps.satellites.value() : 0UL,
                  WiFi.softAPgetStationNum());
  }
}
