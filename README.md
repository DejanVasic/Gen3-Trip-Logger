# 🚗 Gen 3 Smart Trip Logger & Dashboard

Why wouldn’t Toyota Prius Gen 3 have features similar to MyToyota App? <br>
This ESP32-based device interfaces with the vehicle's CAN bus, enabling monitoring, logging, and display of trip data. It also enhances convenience and safety through automation features.
For logging it uses Google sheet while the dashboard display is reachable as local website (using your phone as a hotspot).

---

## ✨ Features

- **CAN Bus Integration**: Reads and writes data to the Prius Gen 3 CAN bus.
- **Trip Data Logging**:
  - Odometer
  - GPS position
  - Trip duration and distance (total and EV-only)
  - Fuel consumption
  - Tank level
- **Web Dashboard** (hosted on ESP32, served from SD card with gzip compression):
  - RPM (with calculated engine load shown inside the radial gauge), tank level, estimated DTE, steering angle, engine and inverter temperature
  - Average fuel consumption, trip distance and trip duration
  - Real speed, cruise-control memorized speed, and average trip speed (with reset for toll-to-toll average speeding measurement)
  - Traction battery: live current (charge/discharge), per-block voltages of all 14 NiMH blocks visualized as a stacked bar, and auxiliary 12V battery voltage
  - Cabin temperature
  - Diagnostic page at `/debug` (free heap, last reset reason, uptime, Wi-Fi RSSI, SSE client count, presence/size of the offline-buffered `GPSdata.tsv`, last upload timing, fuel-cut status, door/window status, per-block internal resistance)
- **Google Sheets Sync**:
  - Uploads trip data to Google Sheets or temporarily saves them to SD card if there is no internet link detected
  - Generates trip maps using Google Apps Script
  - On tank refill logs previous drive data
- **Android Head Unit App**:
  - Displays the ESP32 web page dashboard in fullscreen constructing its IP address by replacing the last octet with a known static value
  - Tap the logo to open `/debug`; the device's back button navigates within the WebView (returns to the dashboard from `/debug`); pull-to-refresh reloads the current page (or rediscovers the IP when on root or on the error screen)
  - Shows currently playing song title from local audio player
- **Smart Locking**:
  - Auto-locks doors when driving starts
  - Auto-unlocks when parked or when collision is detected (rapid deceleration also turns on hazard lights)
- **Other**:
  - Audible alert if the car is started with wheels not in a almost straight position
  - Audible alert on car turn-off if any window is left open (longer alert for rear windows, shorter for front)
  - Blinks the hazard lights once when the first door is opened after the car is turned off, providing an immediate warning to other drivers
  - Dims aftermarket HeadUnit when Prius dashboard is dimmed too

---

## 🛠 Hardware Requirements (I used those modules)

- ESP32 microcontroller (Wemos Lolin32)
- CAN transceiver module (SN65HVD230)
- GPS module (Ublox NEO-7M-000)
- SD card module
- DC/DC converter (Mini560)
- Optocoupler (ASSR-1228)
- passive piezo buzzer from some PC motherboard
---


## 💻 Software Components

- **Arduino Firmware** for ESP32
- **Web Interface** (HTML/CSS/JS served by ESP32)
- **Google Apps Script** for Sheets integration and map generation
- **Android App** for head unit display. Any web browser with javascript support can be used

---

## 📶 WiFi Connection

To log data to Google Sheets, the device needs an active Wi-Fi connection. If no connection is available, data will be temporarily stored on the SD card until connectivity is restored.<br>
For that you can use your phone as a hotspot. If you have Bluetooth device in your car, the phone can be automatized to turn on hotspot once its connected to that Bluetooth and disconnect when not. (e.g. Samsung have "Modes and Routines" to turn on Hotspot when Bluetooth device is connected).<br>
When you create [Google service account](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/Google-spreadsheet/README.md) add to that .json file fields: "_spreadsheetId_", "_cell_", "_SSID_", "_WiFipass_", "_Hostname_" like in [sample file](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/firmware/SDcard/Settings.json) and place it on SD card as Settings.json.<br>
- **spreadsheetId** is ID of Google sheet to which you log trip data.<br>Read it from url (e.g. ...spreadsheets/d/**this_is_sheet_id_for_copying_it**/edit...).
- **cell** is spreadsheet name and first table cell address 
- **SSID** is WiFi network name (of your phone or garage/house AP)
- **WiFipass** is security key (WPA3 isn't supported since that library uses too much program storage space).
- **Hostname** is trial to use it with mdns Arduino library to access ESP32 Webpage (dashboard) using its hostname and bonjour browser but since it isn't reliable you can use this [android app](https://github.com/DejanVasic/Gen3-Trip-Logger/tree/master/app/release)

**![Ssreenshot](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/Screenshot.jpg)**
