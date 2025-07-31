# ESP32 CAN GEN 3 Trip Logger

This project is a custom-built data logger using an ESP32 microcontroller. It reads data from a CAN bus, logs GPS coordinates, and stores the information on an SD card. You can adapt .ino to suite on different hardware configurations.

## 📦 Hardware Components

The device is built using the following components:

- **ESP32 Microcontroller** – Wemos Lolin32
- **CAN Transceiver Module** – SN65HVD230
- **GPS Module** – Ublox NEO-7M-000
- **SD Card Module** – For data storage
- **DC/DC Converter** – Mini560 (for power regulation)
- **Optocoupler** – ASSR-1228 (for signal isolation)

> ⚠️ **Note:** You do not need to follow this exact hardware design. You can use different ESP32 boards or modules. However, you must update the firmware accordingly.

## 🛠️ Firmware Customization

The firmware is located in folder [`firmware.ino`](https://github.com/DejanVasic/Gen3-Trip-Logger/tree/master/firmware). If you use different hardware (e.g., another ESP32 variant or different pin assignments), you **must modify** the following sections:

- **Pin Definitions** – Update GPIO numbers for CAN, GPS, SD card, etc.
- **Serial Interfaces** – Adjust UART settings for GPS communication.
- **SPI Settings** – Ensure compatibility with your SD card module.
- **Power Management** – If using a different power setup, review voltage levels and current requirements.

## 📂 Data Format

Logged data is temporarry stored on the SD card in TSV format, including:

- Timestamp
- Odometer
- Tank level
- Date time (if connected to the internet)
- GPS coordinates (latitude, longitude, altitude)
- Current trip serial, EV and total time
- Current trip Fuel Consumption, EV and total distance
When connected to the internet, those data are uploaded to Google spreadsheet and removed from SD card
## 📂 Realtime Data as webpage
- Timestamp
- RPM
- Tank level
- Current trip Time, Distance and Consumptionm
- Car cabin air temperature
- Engine temperature
- Steering Angle
- Real speed

## 📷 Schematic Diagram

![Schematic Diagram](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/hardware/Gen3logger.png)
