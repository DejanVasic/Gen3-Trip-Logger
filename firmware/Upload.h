/*

  Adapted from the examples of the Libraryes for Arduino devices
  Some handy tutorials: 
 
  https://github.com/mobizt/ESP-Google-Sheet-Client
  https://randomnerdtutorials.com/esp32-neo-6m-gps-module-arduino/
  https://randomnerdtutorials.com/esp32-microsd-card-arduino/
  https://RandomNerdTutorials.com/esp32-web-server-gauges/
  https://randomnerdtutorials.com/esp32-datalogging-google-sheets/
  https://electropeak.com/learn/sending-data-from-esp32-or-esp8266-to-google-sheets-2-methods/?srsltid=AfmBOopzhUqGB4WF2dHTu3gyPGgh2NFoK30_Kxx5BiYAGNEQSFbGRDLr

*/

//#include <ArduinoJson.h>
#include <WiFi.h>
#include "HardwareSerial.h"
//#include "time.h"
#include <ESP_Google_Sheet_Client.h>  //https://randomnerdtutorials.com/esp32-datalogging-google-sheets/
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <TinyGPSPlus.h>
//#include <ESPmDNS.h>
//#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>  //1280*720
//#include "mdns.h"  //it doesn't work as expected so I'll use my app for browsing using IP address
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "driver/rtc_io.h"
#include <HTTPClient.h>
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
#ifndef UPLOAD_H_
#define UPLOAD_H_
#endif
const uint8_t chipSelect = 5;
String WIFI_SSID = "";
String WIFI_PASSWORD = "";
String PROJECT_ID = "";
String CLIENT_EMAIL = "";
char PRIVATE_KEY[2048] = "";
char HOSTNAME[32] = "";
String spreadsheetId = "";
String cell = "";  //Spreadsheet name and first table cell
//unsigned long epochTime;
//unsigned long millisOnEpoch;
char dateTime[20] = { 0 };
//String dateTime = "0";
volatile double latitude = 0.0;
volatile double longitude = 0.0;
volatile float altitude = 0.0;
//bool mDNSInitialized = false;
bool uploading = false;
bool noSettings = true;
uint8_t wiFiConnection = 0;  //0 never connected, 1 first time connected, 2 reconnected
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");
// Json Variable to Hold Sensor Readings
FirebaseJsonData readings;
// Token Callback function
void tokenStatusCallback(TokenInfo info);
void lockDoors();  // Function declaration
void checkSDVersion();

// Function that gets current epoch time in 3 attempts
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  uint8_t step = 0;
  while (!getLocalTime(&timeinfo) && step <= 1) {
    step++;
    vTaskDelay(50 / portTICK_RATE_MS);
    Serial.print(F(" step: "));
    Serial.println(step);
  }
  time(&now);
  return now;
}

String getCanReadings() {
  FirebaseJson json;  // local, re-entrant

  // Snapshot shared globals in a short critical section
  portENTER_CRITICAL(&canReadMux);
  unsigned long msec_local = msec;
  uint16_t rpm_local = rpm;
  float tank_local = tankLitters;
  int8_t temp_local = tempC;
  int8_t invTemp_local = invTemp;
  float dist_local = tripDistance;
  float cons_local = tripConsumption;
  int16_t sAng_local = steeringAngle;
  uint8_t numSat_local = numSat;
  uint8_t speed_local = speed;
  uint8_t ccSpeed_local = ccSpeed;
  uint16_t tc_local = tripCounter;
  uint8_t fss_local = fss;
  int8_t tempRoomC_local = tempRoomC;
  portEXIT_CRITICAL(&canReadMux);

  // Build JSON from the local snapshot
  json.set("rpm", rpm_local);
  json.set("tank", tank_local);
  json.set("temp", temp_local);
  json.set("invTemp", invTemp_local);
  json.set("dist", dist_local);
  json.set("cons", cons_local);
  json.set("msec", msec_local);
  json.set("sAng", sAng_local);
  json.set("numSat", numSat_local);
  json.set("speed", speed_local);
  json.set("ccSpeed", ccSpeed_local);
  json.set("tripCounter", (tc_local + fss_local / 10.0));
  json.set("tempRoomC", tempRoomC_local);

  String jsonString;
  if (json.toString(jsonString)) {
    return jsonString;
  }
  // Fallback
  Serial.println("getCanReadings: json.toString() failed");
  return "{}";
}

void readSettings() {
  if (SD.exists("/Settings.json")) {
    File Settings = SD.open("/Settings.json", FILE_READ);
    if (!Settings) {
      Serial.println(F("Failed to open Settings file for reading"));
      return;
    }

    String jsonString;
    while (Settings.available()) {
      jsonString += (char)Settings.read();
    }
    Settings.close();

    // Create a FirebaseJson object
    FirebaseJson json;
    json.setJsonData(jsonString);
    FirebaseJsonData jsonData;
    // Get values from JSON
    String tempChar;
    if (json.get(jsonData, "SSID")) WIFI_SSID = jsonData.stringValue;
    if (json.get(jsonData, "WiFipass")) WIFI_PASSWORD = jsonData.stringValue;
    if (json.get(jsonData, "spreadsheetId")) spreadsheetId = jsonData.stringValue;
    if (json.get(jsonData, "cell")) cell = jsonData.stringValue;
    if (json.get(jsonData, "Hostname")) tempChar = jsonData.stringValue;
    tempChar.toCharArray(HOSTNAME, sizeof(HOSTNAME));
    if (json.get(jsonData, "private_key")) tempChar = jsonData.stringValue;
    tempChar.replace("\\n", "\n");
    tempChar.toCharArray(PRIVATE_KEY, sizeof(PRIVATE_KEY));
    if (json.get(jsonData, "project_id")) PROJECT_ID = jsonData.stringValue;
    if (json.get(jsonData, "client_email")) CLIENT_EMAIL = jsonData.stringValue;
    noSettings = false;
  } else {
    Serial.println(F("Settings.json does not exist"));
  }
}

//      0           1           2           3             4            5      6    7       8          9               10                11              12           13          14
//Unix Date,	Milliseconds,	Odometer Km,	Tank l,	GPS DateTime (UTC),	LAT,	LONG,	ALT,	TRIP #,	Trip distance,	Trip consumption	|Trip time,  Internet DateTime,	Av speed,	Av Consumption

void write2SD() {
  char dtbuf[20] = { 0 };
  portENTER_CRITICAL(&canReadMux);
  unsigned long msec_local = msec;
  uint32_t odom_local = odometer;
  float tank_local = tankLitters;
  //dateTime.toCharArray(dtbuf, sizeof(dtbuf));
  //char dtbuf[20] = dateTime;
  strncpy(dtbuf, dateTime, sizeof(dtbuf));
  dtbuf[sizeof(dtbuf) - 1] = '\0';
  double latitude_local = latitude;
  double longitude_local = longitude;
  float altitude_local = altitude;
  uint16_t tc_local = tripCounter;
  float dist_local = tripDistance;
  float cons_local = tripConsumption;
  float tdEV_local = tripDistanceEV;
  unsigned long msecEV_local = msecEV;
  portEXIT_CRITICAL(&canReadMux);

  // Open the file in append mode
  File file = SD.open("/GPSdata.tsv", FILE_APPEND);
  if (file) {
    // Write the data to the file
    file.print(getTime());
    file.print("\t");
    file.print(msec_local);
    file.print("\t");
    file.print(odom_local);
    file.print("\t");
    file.print(tank_local);
    file.print("\t");
    file.print(dtbuf);
    file.print("\t");
    file.print(latitude_local, 9);
    file.print("\t");
    file.print(longitude_local, 9);
    file.print("\t");
    file.print(altitude_local);
    file.print("\t");
    file.print(tc_local);
    file.print("\t");
    file.print(dist_local);
    file.print("\t");
    file.print(cons_local);
    file.print("\t");
    file.print(tdEV_local);
    file.print("\t");
    file.print(msecEV_local);
    file.println("");
    file.close();
    Serial.println(F("Data appended to file."));
  } else {
    Serial.println(F("Error opening file for append."));
    tone(buzzer, 2000, 100);
    vTaskDelay(150 / portTICK_RATE_MS);
    tone(buzzer, 2000, 100);
    vTaskDelay(150 / portTICK_RATE_MS);
    tone(buzzer, 2000, 100);
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}


void connect2WIFI() {

  if (WiFi.status() == WL_CONNECTED || WIFI_SSID == "") {  //&& mDNSInitialized) {
    if (wiFiConnection == 0) {
      wiFiConnection = 1;
      Serial.print(F("RRSI: "));
      Serial.println(WiFi.RSSI());
      Serial.print(F(" CONNECTED: "));
      Serial.println(WiFi.localIP());
      configTime(0, 0, "0.europe.pool.ntp.org", "rs.pool.ntp.org", "pool.ntp.org");
    }
    return;
  } else {
    //mDNSInitialized = false;
    if (WiFi.status() != WL_CONNECTED) {

      WiFi.disconnect(true);  // Clear any stale connection
      vTaskDelay(100 / portTICK_RATE_MS);
      WiFi.setHostname(HOSTNAME);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      if (WiFi.waitForConnectResult(3000) != WL_CONNECTED) {
        Serial.print(F("WIFI connecting to "));
        Serial.println(WIFI_SSID);
      }
      /*       if (WiFi.status() == WL_CONNECTED) {

      } */
    }
  }
}

void go2Sleep() {
  digitalWrite(gpsOn, HIGH);
  gpio_hold_en(gpsOn);
  WiFi.disconnect(true);
  //sendUBX(UBX_POWER_DOWN, sizeof(UBX_POWER_DOWN));
  //gpsSerial.println("$PMTK161,0*28");  // Standby mode command
  esp_sleep_enable_timer_wakeup(48 * 60 * 60 * 1000000ULL);  // 2 days in μs
  //esp_sleep_enable_timer_wakeup(60000000);  // test
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_34, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.println(F("DREMAM"));
  //if (digitalRead(IGNin) == LOW) {
  //  esp_deep_sleep(1000);
  //} else {
  esp_deep_sleep_start();
  //}
}

void go2DeepSleep() {
  WiFi.disconnect(true);
  gpio_hold_dis(gpsOn);
  digitalWrite(gpsOn, LOW);
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_34, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}

void upLoad2Google(void* parameter) {
  //if (msec > uploadms + 10000) { upLoad2Google(NULL); }  // less than 10 secondes passed after last upload //upLoadTask();

  if (uploading) return;
  uploading = true;
  vTaskDelay(1 / portTICK_RATE_MS);
  //connect2WIFI();
  if (WiFi.status() != WL_CONNECTED) {
    write2SD();
    uploading = false;
    if (digitalRead(IGNin) == LOW) {
      tone(buzzer, 2000, 100);
      vTaskDelay(150 / portTICK_RATE_MS);
      tone(buzzer, 2000, 100);
      go2Sleep();
    }
    GoogleTask = NULL;
    vTaskDelete(NULL);
  }
  //connect 2 wifi and get time
  // Configure time and Google Sheets
  GSheet.reset();
  GSheet.setTokenCallback(tokenStatusCallback);
  GSheet.setPrerefreshSeconds(180);

  //GSheet.begin("/Settings.json", esp_google_sheet_file_storage_type_sd);
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
  GSheet.ready();

  FirebaseJson response;
  FirebaseJson valueRange;
  valueRange.add("majorDimension", "COLUMNS");
  char dtbuf[20];
  portENTER_CRITICAL(&canReadMux);
  unsigned long msec_local = msec;
  uint32_t odom_local = odometer;
  float tank_local = tankLitters;
  strncpy(dtbuf, dateTime, sizeof(dtbuf));
  dtbuf[sizeof(dtbuf) - 1] = '\0';
  double latitude_local = latitude;
  double longitude_local = longitude;
  float altitude_local = altitude;
  uint16_t tc_local = tripCounter;
  float dist_local = tripDistance;
  float cons_local = tripConsumption;
  float tdEV_local = tripDistanceEV;
  unsigned long msecEV_local = msecEV;
  portEXIT_CRITICAL(&canReadMux);

  // Set live data first
  valueRange.set("values/[0]/[0]", getTime());
  valueRange.set("values/[1]/[0]", msec_local);
  valueRange.set("values/[2]/[0]", odom_local);
  valueRange.set("values/[3]/[0]", tank_local);
  valueRange.set("values/[4]/[0]", dtbuf);
  valueRange.set("values/[5]/[0]", latitude_local);
  valueRange.set("values/[6]/[0]", longitude_local);
  valueRange.set("values/[7]/[0]", altitude_local);
  valueRange.set("values/[8]/[0]", tc_local);
  valueRange.set("values/[9]/[0]", dist_local);
  valueRange.set("values/[10]/[0]", cons_local);
  valueRange.set("values/[11]/[0]", tdEV_local);
  valueRange.set("values/[12]/[0]", msecEV_local);
  uint8_t liveData = 1;
  bool gUp = false;
  uint32_t totalRowCount = 0;

  if (SD.exists("/GPSdata.tsv")) {
    File dataFile = SD.open("/GPSdata.tsv");
    while (GSheet.ready()) {
      String gData;
      uint8_t rowCount = 0;                            // Reset row count for each batch
      uint8_t row = liveData;                          // Reset row for each batch
      while (rowCount < 50 && dataFile.available()) {  // Read 50 lines
        String line = dataFile.readStringUntil('\r');  // Read until newline
        line.replace("\n", "");                        // Remove carriage return
        if (line.length() < 13) { break; }             // Only process non-empty lines
        uint8_t col = 0;
        uint8_t startIndex = 0;
        uint8_t tabIndex = line.indexOf('\t');
        while (tabIndex >= 0 && col < 13) {  // Ensure we don't exceed the number of columns
          gData = line.substring(startIndex, tabIndex);
          if (col != 4) {  //change decimal separator from . to , in  !date columns
            gData.replace(".", ",");
          }
          valueRange.set("values/[" + String(col) + "]/[" + String(row) + "]", gData);
          startIndex = tabIndex + 1;  // Move past the tab character
          tabIndex = line.indexOf('\t', startIndex);
          col++;
        }
        row++;
        rowCount++;
        totalRowCount++;
      }
      liveData = 0;

      // If no lines were read, break the loop
      if (rowCount == 0) { break; }

      Serial.print(F("-- Data uploading . "));

      gUp = GSheet.values.append(&response, spreadsheetId, cell, &valueRange);
      if (gUp) {
        valueRange.clear();
        valueRange.add("majorDimension", "COLUMNS");
        Serial.print(String(rowCount) + "/" + String(totalRowCount));
        Serial.println(F(" . successfull --"));
      } else {
        Serial.println(F("-- Upload failed!, exiting loop --"));
        Serial.println(GSheet.errorReason());
        break;  // Exit if upload fails
      }
    }
    dataFile.close();
  } else {
    Serial.print(F("-- Live data uploading . "));
    vTaskDelay(10 / portTICK_RATE_MS);  //watchdog tick

    GSheet.ready();
    if (GSheet.ready()) gUp = GSheet.values.append(&response, spreadsheetId, cell, &valueRange);
    if (gUp) {
      valueRange.clear();
      Serial.println(F(". successfull --"));
    } else {
      Serial.println(F(". failed! --"));
      Serial.println(GSheet.errorReason());
    }
  }
  if (gUp) {
    if (SD.exists("/GPSdata.tsv")) {
      tone(buzzer, 2000, 100);
      valueRange.clear();
      SD.remove("/GPSdata.tsv");
      if (!SD.exists("/GPSdata.tsv")) Serial.println(F("File /GPSdata.tsv deleted successfully"));
    }
  } else {
    Serial.println(GSheet.errorReason());
    write2SD();
    tone(buzzer, 2000, 100);
    vTaskDelay(150 / portTICK_RATE_MS);
    tone(buzzer, 2000, 100);
    vTaskDelay(100 / portTICK_RATE_MS);
  }
  uploading = false;
  if (digitalRead(IGNin) == LOW) {
    go2Sleep();
  }
  GoogleTask = NULL;
  vTaskDelete(NULL);
}


void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_error) {
    GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
  } else {
    GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
  }
}

void formatDateTimeBuf(char* out, size_t outSize, TinyGPSDate date, TinyGPSTime time) {
  if (!out || outSize == 0) return;
  // snprintf ensures no overflow and NUL-termination
  snprintf(out, outSize, "%02d.%02d.%04d %02d:%02d:%02d",
           date.day(), date.month(), date.year(),
           time.hour(), time.minute(), time.second());
}

/* String formatDateTime(TinyGPSDate date, TinyGPSTime time) {
  char dateTime[20];
  sprintf(dateTime, "%02d.%02d.%04d %02d:%02d:%02d",
          date.day(), date.month(), date.year(),
          time.hour(), time.minute(), time.second());
  return String(dateTime);
}
 */

void webGps(void* parameter) {

  connect2WIFI();
  static uint32_t sse_id = 0;
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SD, "/index.html", "text/html");
  });
  //server.serveStatic("/", SD, "/");
  server.serveStatic("/", SD, "/")
    .setCacheControl("max-age=28800")  //8h
    .setLastModified("Mon, 01 Jan 2024 00:00:00 GMT");
  // Request for the latest data
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = getCanReadings();
    request->send(200, "application/json", json);
    //json = String();
  });
  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Last ID: %u\n", client->lastId());
    }

    client->send("hello!", NULL, sse_id, 10000);
  });


  server.addHandler(&events);
  server.begin();
  while (true) {
    if (!sse_id) Serial.println(" - Web loop start -");
    sse_id++;
    String payload = getCanReadings();
    events.send(payload.c_str(), "new_readings", sse_id);
    //events.send(getCanReadings().c_str(), "new_readings", msec);
    while (gpsSerial.available() > 0) {  //&& getGPS
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isUpdated()) {
      char tmp[20];
      formatDateTimeBuf(tmp, sizeof tmp, gps.date, gps.time);
      portENTER_CRITICAL(&canReadMux);
      // safe copy into shared global buffer
      strncpy(dateTime, tmp, sizeof(dateTime));
      dateTime[sizeof(dateTime) - 1] = '\0';
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      altitude = gps.altitude.meters();
      //dateTime = formatDateTime(gps.date, gps.time);
      numSat = gps.satellites.value();
      portEXIT_CRITICAL(&canReadMux);
    }

    vTaskDelay(500 / portTICK_RATE_MS);
    connect2WIFI();
    if (wiFiConnection == 1) checkSDVersion();
  }
}


bool downloadFile(const char* serverPath, const char* localPath) {
  Serial.print("Downloading: ");
  Serial.println(serverPath);

  // Initialize HTTP client
  HTTPClient http;
  http.begin(serverPath);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {  // Check for successful download
    Serial.println("Download started...");

    // Open file on SD card for writing (overwrites if it exists)
    File file = SD.open(localPath, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing on SD card.");
      http.end();
      return false;
    }

    // Get the file size
    int len = http.getSize();
    // Get the stream of data
    WiFiClient* stream = http.getStreamPtr();

    // Read data from the stream and write to the file
    int totalBytesWritten = 0;
    byte buff[1024] = { 0 };
    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int bytesRead = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        file.write(buff, bytesRead);
        totalBytesWritten += bytesRead;
        if (len > 0) {
          len -= bytesRead;
        }
      }
    }
    Serial.printf("Downloaded and written %d bytes.\n", totalBytesWritten);
    file.close();
    http.end();
    return true;
  } else {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

void checkSDVersion() {
  wiFiConnection = 2;
  Serial.println("Checking for New version.");

  const char* baseUrl = "https://github.com/DejanVasic/Gen3-Trip-Logger/tree/master/firmware/SDcard";
  const char* filesToDownload[] = {
    "/script.js",
    "/index.html",
    "/style.css",
    "/version.txt"
  };
  String version = "";
  if (SD.exists(String(filesToDownload[3]))) {
    File file = SD.open(String(filesToDownload[3]), FILE_READ);
    version = file.readStringUntil('\n');
    version.trim();  // Remove any whitespace
    file.close();
    Serial.printf("Local version: %s\n", version.c_str());
  }

  HTTPClient http;
  http.begin(String(baseUrl) + String(filesToDownload[3]));
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String serverVersion = http.getString();
    serverVersion.trim();
    http.end();

    Serial.printf("Server version: %s\n", serverVersion.c_str());

    // Compare local and server versions
    if (version != serverVersion) {
      Serial.println("New version found! Starting download process.");

      bool allFilesDownloaded = true;
      // Iterate through the list of other files and download each one.
      for (int i = 0; i < 4; i++) {
        String fullUrl = String(baseUrl) + String(filesToDownload[i]);
        String sdPath = String(filesToDownload[i]);

        if (!downloadFile(fullUrl.c_str(), sdPath.c_str())) {
          Serial.printf("Failed to download file: %s\n", filesToDownload[i]);
          allFilesDownloaded = false;
          break;  // Exit the loop on the first download failure
        } else {
          Serial.printf("Successfully downloaded: %s\n", filesToDownload[i]);
        }
      }

    } else {
      Serial.println("No update needed. Device is up to date.");
    }
  } else {
    Serial.printf("Failed to check for server version, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
  }
}
