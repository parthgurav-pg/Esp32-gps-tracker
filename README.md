# ESP32 Live GPS Tracker

A real-time GPS tracking system built with an **ESP32**, **NEO-6M GPS module**, and **I²C OLED display**. View live coordinates and movement on your phone using either **Wi-Fi** or **Bluetooth Low Energy (BLE)**.

Both methods use the same hardware connections; only the ESP32 firmware and phone connection method change.

## Components

- ESP32 development board (ESP32-WROOM-32)
- GY-GPS6MV2 / NEO-6M GPS module with antenna
- SSD1306 I²C OLED display (128 × 64, 4-pin)
- Jumper wires and USB power supply

## Connections

| Component | Pin | ESP32 |
|---|---|---|
| GPS | VCC | VIN / 5V* |
| GPS | GND | GND |
| GPS | TX | GPIO16 (RX2) |
| GPS | RX | Not connected |
| OLED | VCC | 3V3 |
| OLED | GND | GND |
| OLED | SDA | GPIO21 |
| OLED | SCL | GPIO22 |

*Check that your GPS breakout supports 5V input and that its UART TX output is safe for 3.3V ESP32 GPIO.*

**Arduino libraries:** TinyGPSPlus, Adafruit GFX Library, and Adafruit SSD1306. Select **ESP32 Dev Module** in Arduino IDE.

## Method 1 — Wi-Fi Tracking

**Files:** [`ESP32_GPS_WiFi_OLED.ino`](ESP32_GPS_WiFi_OLED.ino)

1. Open `ESP32_GPS_WiFi_OLED.ino` in Arduino IDE and upload it to the ESP32.
2. Connect your phone to the ESP32 Wi-Fi network:
   - **SSID:** `ESP32-GPS-Tracker`
   - **Password:** `gps12345`
3. Open **http://192.168.4.1** in your phone's browser.
4. View live GPS coordinates, satellite status, and tracking information on the built-in interactive page.

The OLED displays GPS status and Wi-Fi connection information. GPS readings work without internet; online street-map tiles require internet access. If supported by the uploaded firmware, an offline position plot can be used instead.

## Method 2 — Bluetooth LE Tracking

**Files:** [`ESP32_GPS_BLE_OLED.ino`](ESP32_GPS_BLE_OLED.ino) and [`index.html`](index.html)

1. Upload `ESP32_GPS_BLE_OLED.ino` to the ESP32 using Arduino IDE.
2. Host `index.html` on an **HTTPS** website, such as GitHub Pages (repository **Settings → Pages → Deploy from a branch → main / root**).
3. On an Android phone, open the hosted webpage in **Chrome** with Bluetooth enabled.
4. Select **Connect ESP32**, choose your ESP32 from the BLE device picker, and connect.
5. View live GPS coordinates, satellite information, speed, and movement on the interactive map.

The OLED shows GPS fix, satellite count, and BLE connection status. GPS data travels directly over Bluetooth; internet is needed to load the hosted webpage and online map tiles.

## Notes

- For best GPS reception, place the antenna outdoors with a clear view of the sky.
- The NEO-6M provides standard GPS positioning, **not centimetre-level RTK accuracy**.
- **Wi-Fi:** convenient for a locally hosted webpage. **BLE:** convenient for keeping the phone's normal internet connection while receiving GPS data.

---

**One tracker. Two wireless connection methods.**
