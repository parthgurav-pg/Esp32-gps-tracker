#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define GPS_RX 16
#define GPS_TX 17
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

// These must match the UUIDs in index.html.
#define SERVICE_UUID "0ad78e5c-1a7a-4b9a-8c15-31a96d9e32c2"
#define GPS_CHAR_UUID "2db8aa52-9017-4a5c-95a0-6f234e01981b"

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
BLECharacteristic *gpsCharacteristic = nullptr;
bool oledReady = false;
volatile bool bleConnected = false;
unsigned long lastUpdate = 0;
unsigned long receivedBytes = 0;
unsigned long lastByteAt = 0;

class TrackerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override { bleConnected = true; }
  void onDisconnect(BLEServer *server) override {
    bleConnected = false;
    BLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Wire.begin(OLED_SDA, OLED_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledReady) {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1); display.setCursor(0, 0);
    display.println("GPS BLE TRACKER"); display.println("Starting..."); display.display();
  } else Serial.println("OLED not found: try 0x3D / check SDA SCL");

  BLEDevice::init("ESP32-GPS-BLE");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new TrackerCallbacks());
  BLEService *service = server->createService(SERVICE_UUID);
  gpsCharacteristic = service->createCharacteristic(
    GPS_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  gpsCharacteristic->addDescriptor(new BLE2902());
  gpsCharacteristic->setValue("WAITING");
  service->start();
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.println("BLE advertising: ESP32-GPS-BLE");
}

void loop() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
    receivedBytes++;
    lastByteAt = millis();
  }
  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  bool dataArriving = receivedBytes > 0 && millis() - lastByteAt < 3000;
  bool fix = dataArriving && gps.location.isValid() && gps.location.age() < 3000;
  int sats = (dataArriving && gps.satellites.isValid() && gps.satellites.age() < 10000)
      ? gps.satellites.value() : 0;
  String payload = String(fix ? "1" : "0") + "," +
    (fix ? String(gps.location.lat(), 6) : "0") + "," +
    (fix ? String(gps.location.lng(), 6) : "0") + "," +
    String(sats) + "," +
    (fix && gps.speed.isValid() && gps.speed.age() < 3000 ? String(gps.speed.kmph(), 1) : "0") + "," +
    (fix && gps.altitude.isValid() && gps.altitude.age() < 3000 ? String(gps.altitude.meters(), 1) : "0") + "," +
    (fix && gps.hdop.isValid() && gps.hdop.age() < 3000 ? String(gps.hdop.hdop(), 1) : "0") + "," +
    String(dataArriving ? "1" : "0");

  gpsCharacteristic->setValue(payload.c_str());
  if (bleConnected) gpsCharacteristic->notify();
  Serial.printf("GPS %s | SAT %d | BLE %s | %s\n",
    fix ? "FIX" : (dataArriving ? "SEARCHING" : "NO DATA"),
    sats, bleConnected ? "CONNECTED" : "WAITING", payload.c_str());

  if (oledReady) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0); display.println("GPS BLE TRACKER");
    display.print("GPS: "); display.println(fix ? "FIX OK" : (dataArriving ? "SEARCHING" : "NO DATA"));
    display.print("SAT: "); display.println(sats);
    display.print("BLE: "); display.println(bleConnected ? "CONNECTED" : "WAITING");
    if (fix) {
      display.print("LAT:"); display.println(gps.location.lat(), 6);
      display.print("LON:"); display.println(gps.location.lng(), 6);
    } else display.println("Waiting for GPS fix");
    display.display();
  }
}
