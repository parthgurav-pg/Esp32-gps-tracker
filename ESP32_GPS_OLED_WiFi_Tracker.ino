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
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Live GPS Tracker</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#101827;color:#eef5ff;font-family:system-ui,Arial,sans-serif}
main{max-width:950px;margin:auto;padding:18px}h1{font-size:1.5rem;margin:0 0 6px}
.sub{color:#9db2ce;font-size:.9rem;margin-bottom:16px}.status{padding:12px;border-radius:10px;background:#23324a;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;margin-bottom:14px}
.metric{background:#1c2b42;border-radius:12px;padding:13px;min-width:0}.metric small{display:block;color:#9db2ce}
.metric strong{display:block;font-size:1.13rem;margin-top:5px;overflow-wrap:anywhere}
#map{height:420px;background:#1b293d;border-radius:12px;overflow:hidden;position:relative}
#map .fallback{padding:22px;color:#a8b8cc}button,a.btn{border:0;background:#337cdd;color:white;border-radius:8px;padding:11px 15px;cursor:pointer;text-decoration:none;display:inline-block;margin:10px 7px 0 0}
button:disabled,a.disabled{opacity:.45;pointer-events:none}.note{color:#a9bbd1;font-size:.85rem;line-height:1.5}
@media(max-width:500px){#map{height:350px}.metric strong{font-size:1rem}}
</style>
<!-- Leaflet requires internet to load. GPS values below work without it. -->
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
</head><body><main>
<h1>🛰 ESP32 Live GPS Tracker</h1><div class="sub">GPS data updates every second over local ESP32 Wi-Fi</div>
<div id="status" class="status">Connecting to ESP32...</div>
<div class="grid">
<div class="metric"><small>Latitude</small><strong id="lat">--</strong></div>
<div class="metric"><small>Longitude</small><strong id="lng">--</strong></div>
<div class="metric"><small>Satellites</small><strong id="sat">--</strong></div>
<div class="metric"><small>Speed</small><strong id="speed">--</strong></div>
<div class="metric"><small>Altitude</small><strong id="alt">--</strong></div>
<div class="metric"><small>HDOP</small><strong id="hdop">--</strong></div>
</div>
<div id="map"><div class="fallback">Map needs internet access. GPS coordinates above work without internet.</div></div>
<button id="follow" onclick="toggleFollow()">Follow: ON</button>
<button onclick="clearTrail()">Clear trail</button>
<a class="btn disabled" id="mapsLink" href="#" target="_blank" rel="noopener">Open in Google Maps</a>
<p class="note">The map background needs online tiles (Leaflet/OpenStreetMap). If your phone has no internet while connected to ESP32 Wi-Fi, use the coordinates above. Wi-Fi client count is shown on the OLED. GPS is not centimetre-accurate.</p>
</main><script>
let map=null,marker=null,trail=null,follow=true,first=true,points=[];
const el=id=>document.getElementById(id);
if(window.L){
  el('map').innerHTML='';
  map=L.map('map').setView([20.5937,78.9629],4);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{
    maxZoom:19,attribution:'&copy; OpenStreetMap contributors'
  }).addTo(map);
  trail=L.polyline([],{color:'#328bff',weight:4}).addTo(map);
} else {el('map').innerHTML='<div class="fallback">Online map library unavailable. GPS data will still update.</div>';}
function toggleFollow(){follow=!follow;el('follow').textContent='Follow: '+(follow?'ON':'OFF');}
function clearTrail(){points=[];if(trail)trail.setLatLngs([]);}
function show(id,value){el(id).textContent=value;}
async function update(){
 try{
  const response=await fetch('/gps',{cache:'no-store'});
  if(!response.ok)throw Error('HTTP '+response.status);
  const g=await response.json();
  show('sat',g.satellites===null?'--':g.satellites);
  show('hdop',g.hdop===null?'--':g.hdop);
  if(g.valid){
   show('lat',g.lat.toFixed(6));show('lng',g.lng.toFixed(6));
   show('speed',g.speed===null?'--':g.speed.toFixed(1)+' km/h');
   show('alt',g.altitude===null?'--':g.altitude.toFixed(1)+' m');
   el('status').textContent='GPS FIX OK • Live coordinates • Wi-Fi connected';
   const p=[g.lat,g.lng];
   if(map){
     if(!marker)marker=L.marker(p).addTo(map).bindPopup('ESP32 GPS location');
     else marker.setLatLng(p);
     if(first||follow)map.setView(p,first?17:map.getZoom());
     if(!points.length||Math.abs(points[points.length-1][0]-p[0])>0.000002||Math.abs(points[points.length-1][1]-p[1])>0.000002){
       points.push(p);if(points.length>500)points.shift();trail.setLatLngs(points);
     }
   }
   first=false;
   el('mapsLink').href='https://www.google.com/maps?q='+g.lat+','+g.lng;
   el('mapsLink').classList.remove('disabled');
  }else{
   el('status').textContent=g.receiving?'GPS data received • Waiting for valid satellite fix':'No recent GPS serial data • Check GPS TX/GPIO16';
   show('lat','--');show('lng','--');show('speed','--');show('alt','--');
  }
 }catch(e){el('status').textContent='ESP32 connection lost: '+e.message;}
}
update();setInterval(update,1000);
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
