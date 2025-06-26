# Android ESP32 Web Browser

This Android application is a fullscreen web browser (without an address bar) developed in Kotlin using Android Studio. It is designed to display a webpage served by an ESP32 device connected to the same Wi-Fi network, typically a mobile hotspot.

## Features

- **Fullscreen WebView**: Displays a webpage in fullscreen mode, leaving only the phone's status bar visible.
- **ESP32 IP Discovery**: Automatically discovers the phone's IP address and modifies the last octet to connect to the ESP32 device.
- **Persistent Configuration**: Remembers the last octet entered by the user on the first run.
- **Reconnect Logic**: Attempts to reconnect if the ESP32 web server is unreachable.
- **Now Playing Info**: Displays the currently playing song's artist and title from the local audio player.
- **Notification Access**: Requires user permission to read notifications for displaying song information.

## Usage

1. **Initial Setup**:
   - On the first run, the user is prompted to enter the last octet of the ESP32 device's IP address.
   - This value is stored for future use.

2. **WebView Display**:
   - The app constructs the ESP32 IP address using the phone's IP and the stored last octet.
   - It attempts to connect to the ESP32 web server and display the webpage.

3. **Notification Access**:
   - The app requests permission to read notifications.
   - This is required to display the currently playing song's artist and title.

4. **Changing ESP32 Device**:
   - If the ESP32 device is changed and the IP address differs, the user must clear the app data.
   - On the next run, the app will prompt for a new last octet.

## Permissions

- **Notification Access**: Required to read and display song information from the local audio player.

## Target Use Case

This app is primarily designed to run on Android head units in cars, providing a seamless interface to interact with an ESP32 device over a local Wi-Fi network.

**![SampleConnecting.png](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/app/ScreenshotConnecting.jpg)**
**![SampleConnected.png](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/app/ScreenshotConnected.jpg)**
