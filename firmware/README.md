# Gen3-CAN-Trip-Logger

## Project Overview

The `Gen3-CAN-Trip-Logger` is an Arduino-based device designed to interface with a vehicle's CAN (Controller Area Network) bus. Its primary function is to monitor, log, and display various trip-related data. Beyond data logging, the device aims to enhance convenience and safety through automation features. 

The firmware is developed for ESP32 boards, specifically tested with the Wemos LOLIN32, and utilizes several third-party libraries for extended functionality. 

## Features

* **CAN Bus Monitoring:** Intercepts and decodes various CAN messages from the vehicle, including:
    * Odometer readings 
    * Vehicle speed 
    * Light dimmer status 
    * Gear position 
    * Steering wheel angle 
    * Door lock status 
    * Engine RPM 
    * Engine temperature 
    * Fuel tank level 
    * Fuel injection volume 
    * Cabin temperature 

* **Trip Data Logging:**
    * Calculates and logs trip distance, trip consumption, and electric vehicle (EV) trip distance/time. 
    * Stores data on an SD card in a tab-separated values (TSV) format (`GPSdata.tsv`). 
    * Tracks and increments a trip counter stored in EEPROM. 

* **Google Sheet Integration:**
    * Uploads trip data to Google Sheets periodically (every 5 minutes or every 10km, whichever comes first). 
    * Reads Google Sheet API credentials (Project ID, Client Email, Private Key, Spreadsheet ID) from a `Settings.json` file on the SD card. 
    * Includes a retry mechanism for uploads if the initial attempt fails. 

* **GPS Integration:**
    * Utilizes a TinyGPSPlus library with a NEO-6M GPS module (or similar) to acquire location (latitude, longitude, altitude), date, time, and satellite count. 
    * GPS data is used in conjunction with CAN bus data for comprehensive trip logging. 

* **Automation and Safety Features:**
    * **Automatic Door Locking:** Locks doors when speed exceeds 15 km/h. 
    * **Collision Detection:** Unlocks doors if a rapid deceleration (indicating a potential collision) is detected. 
    * **Automatic Door Unlocking:** Unlocks doors when the vehicle is put in "Park" and doors were locked by the system. 
    * **Steering Angle Alarm:** Triggers a buzzer sound once if the steering angle is outside a certain range, potentially indicating an issue or an unusual maneuver. 
    * **Dimmable Output:** Controls an output pin (e.g., for lights) based on the vehicle's dimmer signal. 
    * **Deep Sleep Mode:** Enters a deep sleep mode to conserve power when the ignition is off, with wake-up triggered by ignition or a timer. 

* **Web Server Interface:**
    * Hosts an asynchronous web server on port 80 to display live CAN readings. 
    * Provides an `/events` endpoint for Server-Sent Events (SSE) to push real-time data to connected clients. 

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

### `Settings.json` Configuration

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
