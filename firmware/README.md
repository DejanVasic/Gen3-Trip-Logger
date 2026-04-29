# Gen3-CAN-Trip-Logger

## Project Overview

The `Gen3-CAN-Trip-Logger` is an Arduino-based device designed to interface with a vehicle's CAN (Controller Area Network) bus. Its primary function is to monitor, log, and display various trip-related data. Beyond data logging, the device aims to enhance convenience and safety through automation features.

The firmware is developed for ESP32 boards, specifically tested with the Wemos LOLIN32, and utilizes several third-party libraries for extended functionality.

## Features

* **CAN Bus Monitoring:** Intercepts and decodes various CAN messages from the vehicle, including:
    * Odometer readings
    * Vehicle speed
    * Memorized cruise control speed
    * Light dimmer status
    * Gear position
    * Steering wheel angle
    * Door lock status
    * Windows opened status
    * Engine RPM
    * Engine temperature
    * Inverter coolant temperature
    * Fuel tank level
    * Fuel injection volume
    * Fuel cut status (deceleration / engine-off detection)
    * Calculated engine load
    * Cabin temperature

* **Hybrid Battery Monitoring (ISO-TP):**
    * Real-time traction battery current (charge / discharge in Amps)
    * Per-block voltages for all 14 NiMH battery blocks (read via ISO-TP multi-frame requests)
    * Per-block internal resistance (used for static voltage compensation)
    * Auxiliary 12V battery voltage

* **Trip Data Logging:**
    * Calculates and logs trip distance, trip consumption, and electric vehicle (EV) trip distance/time.
    * Stores data on an SD card in a tab-separated values (TSV) format (`GPSdata.tsv`).
    * Tracks and increments a trip counter stored in EEPROM.

* **Google Sheet Integration:**
    * Uploads trip data to Google Sheets periodically (every 5 minutes or every 10 km, whichever comes first).
    * Reads Google Sheet API credentials (Project ID, Client Email, Private Key, Spreadsheet ID) from a `Settings.json` file on the SD card.
    * Buffers data on SD card when offline and uploads in 50-row batches once Wi-Fi is available again.

* **GPS Integration:**
    * Utilizes a TinyGPSPlus library with a NEO-6M GPS module (or similar) to acquire location (latitude, longitude, altitude), date, time, and satellite count.
    * GPS data is used in conjunction with CAN bus data for comprehensive trip logging.

* **Automation and Safety Features:**
    * **Automatic Door Locking:** Locks doors when speed exceeds 15 km/h.
    * **Collision Detection:** Unlocks doors and turns on hazard lights if a rapid deceleration (drop > 22 km/h in 0.5 s) is detected.
    * **Automatic Door Unlocking:** Unlocks doors when the vehicle is put in "Park" and doors were locked by the system.
    * **Steering Angle Alarm:** Triggers a buzzer sound once on car start if the steering angle is outside a centered range.
    * **Open-Window Alarm on Shutdown:** Triggers a buzzer pattern at ignition-off — a longer alert (10 s) for any rear window left open, a shorter one (5 s) for any front window.
    * **Door Opened Alarm:** Blinks the hazard lights once when the first door is opened after the car is turned off, providing an immediate warning to other drivers.
    * **Dimmable Output:** Controls an output pin (e.g., for HeadUnit lights) based on the vehicle's dimmer signal.
    * **Deep Sleep Mode:** Enters a deep sleep mode to conserve power when the ignition is off, with wake-up triggered by ignition or a timer.

* **Web Server Interface:**
    * Hosts an asynchronous web server on port 80 to display live CAN readings.
    * `/` — main dashboard (HTML/CSS/JS served from SD card; gzip-compressed `.gz` versions are served automatically when available).
    * `/events` — Server-Sent Events (SSE) stream pushing a full JSON snapshot of all readings every 400 ms.
    * `/readings` — one-shot JSON snapshot for polling clients.
    * `/debug` — diagnostic page (HTML, dark theme) showing free heap, last reset reason, uptime, Wi-Fi status, SSE client count, presence and size of `GPSdata.tsv`, last upload attempt timing, fuel-cut status, door/window status (hex), and per-block internal resistance shown as a 4×4 table.

## Performance Notes

The firmware avoids dynamic `String` allocations in the hot paths to keep the heap stable over long drives:
* `getCanReadings()` writes the SSE/HTTP JSON payload directly into a fixed `char` buffer.
* The Google Sheets batch uploader (`upLoad2Google`) parses TSV rows and builds JSON keys with `snprintf` instead of `String` concatenation.
* `readSettings()` pre-reserves the JSON string size before reading the settings file.
* The Arduino `loop()` yields one FreeRTOS tick at the end of each iteration, giving the IDLE task time to reset the watchdog and letting Wi-Fi/CAN driver tasks share the core.

## Dependencies

This project relies on the following third-party Arduino libraries:

* **`ESP_Google_Sheet_Client` by mobizt:** Used for interacting with Google Sheets API to upload data.
* **`TinyGPSPlus` by Mikal Hart:** For parsing NMEA GPS data from a GPS module.
* **`ESPAsyncWebServer`:** An asynchronous web server library for ESP32.
* **`esp32_can.h` by Collin Kidder:** For CAN bus communication on ESP32.
* **`EEPROM.h`:** For non-volatile storage of the trip counter.
* **`WiFi.h`:** For Wi-Fi connectivity.
* **`SPI.h` and `SD.h`:** For SD card communication.
* **`FS.h`:** File system library.

Web Assets (for the built-in server):
* **`orbitron.woff2`:** https://fonts.googleapis.com/css2?family=Orbitron font.
* **`script.js` / `style.css` / `index.html`:** custom dashboard with self-contained SVG/CSS gauges. The current visualization set includes:
    * RPM dual radial gauge with a discrete static color arc indicating low/mid/high RPM zones (green / orange / red), and calculated engine load shown inside the gauge.
    * Vertical bar gauges for fuel tank, engine temperature, inverter temperature, and average fuel consumption.
    * Vertical bar gauge for traction battery current — fills upward from the bottom; bar color indicates direction of flow (charge vs. discharge), and the signed numeric value is shown below the bar.
    * Stacked HV battery cells visualization (14 NiMH blocks) with a pack imbalance indicator below it (peak ΔV between strongest and weakest block, percentage, and which two blocks are involved). The peak resets when the current changes direction so each charge/discharge phase is evaluated on its own.
    * Steering angle indicator and auxiliary 12V battery voltage.

  The dashboard locally extrapolates the trip-time display every second between SSE updates, so the seconds counter advances smoothly even though the SSE interval is 400 ms. No external gauge library is required.
* **`graphics.svg`:** created from icon library https://fontawesome.com/v5/search?ic=free and https://upload.wikimedia.org/wikipedia/commons/8/89/Toyota_Prius_logo.svg

## `Settings.json` Configuration

Sample of a file named `Settings.json` in the root directory of your MicroSD card with the following content. Replace the placeholder values with your actual Wi-Fi credentials and Google Sheet API keys.

```json
{
  "type": "service_account",
  "project_id": "gen3logger",
  "private_key_id": "your_key_id",
  "private_key": "-----BEGIN PRIVATE KEY-----REPLACE WITH YOUR PRIVATE KEY-----END PRIVATE KEY-----\n",
  "client_email": "datalogger@gen3logger.iam.gserviceaccount.com",
  "client_id": "123456789012345678901",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/datalogger%gen3logger.iam.gserviceaccount.com",
  "universe_domain": "googleapis.com",
  "spreadsheetId": "read_from_sharred_google_spreadsheet_url",
  "cell": "Prius!A1",
  "SSID": "S24U",
  "WiFipass": "01234567",
  "Hostname": "gen3logger" 
}
```
