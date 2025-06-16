# 🚗 Gen 3 Smart Trip Logger & Dashboard

An ESP32-based smart automotive companion for the Toyota Prius Gen 3. This device interfaces with the vehicle's CAN bus to monitor, log, and display trip data, while also enhancing convenience and safety with automation features.

**Note:** For Google Sheets logging and dashboard features to function properly, the microcontroller must be connected to the internet via your phone’s hotspot.

---

## ✨ Features

- **CAN Bus Integration**: Reads and writes data to the Prius Gen 3 CAN bus.
- **Trip Data Logging**:
  - Odometer
  - GPS position
  - Trip duration and distance (total and EV-only)
  - Fuel consumption
  - Tank level
- **Web Dashboard** (hosted on ESP32):
  - RPM, Tank level, estimated dte, steering angle, engine and cabin temperature
  - Trip duration, distance, average fuel consumption
  - Real current and average speed (with reset for toll to toll average speeding measurement)
- **Google Sheets Sync**:
  - Uploads trip data to Google Sheets or temporarily saved them to SD card if there is no internet link detected
  - Generates trip maps using Google Apps Script
  - On tank refill logs previous drive data
- **Android Head Unit App**:
  - Displays the ESP32 web page dashboard in fullscreen constructing its IP address by replacing the last octet with a known static value
  - Shows currently playing song title from local audio player
- **Smart Locking**:
  - Auto-locks doors when driving starts
  - Auto-unlocks when parked or when collision is detected
- **Other**:
  - Audible alert if the car is started with wheels not in a almost straight position
  - Dims aftermarket HeadUnit when Prius dashboard is dimmed too

---

## 🛠 Hardware Requirements (I used those modules)

- ESP32 microcontroller (Wemos Lolin32)
- CAN transceiver module (SN65HVD230)
- GPS module (Ublox NEO-7M-000)
- SD card module
- DC/DC converter (Mini560)
- Optocoupler (ASSR-1228)

---

## 💻 Software Components

- **Arduino Firmware** for ESP32
- **Web Interface** (HTML/CSS/JS served by ESP32)
- **Google Apps Script** for Sheets integration and map generation
- **Android App** for head unit display

---

## 🚀 Setup Instructions

1. **Hardware Assembly**:
   - Connect ESP32 to CAN transceiver, SD card reader and GPS module
   - Wire power from vehicle (with voltage regulation) and CAN bus 

2. **Firmware Upload**:
   - Flash the Arduino sketch to ESP32
   - Configure Wi-Fi credentials and CAN bus parameters

3. **Web Dashboard**:
   - Access via phones hotspot to ESP32 web page
   - View real-time trip data

4. **Google Sheets Integration**:
   - Set up Google Sheet and Apps Script
   - Authorize ESP32 to send data via API

5. **Android App**:
   - Install APK on car head unit
   - Launch to view dashboard and music info
