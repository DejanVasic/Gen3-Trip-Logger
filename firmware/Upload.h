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
//#include <AsyncWebResponse.h>
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
AsyncWebServer server(80);
AsyncEventSource events("/events");
FirebaseJsonData readings;
void tokenStatusCallback(TokenInfo info);
void lockDoors();  // Function declaration

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

int getCanReadings(char* out, size_t outSize) {
  if (!out || outSize == 0) return -1;

  portENTER_CRITICAL(&canReadMux);
  unsigned long m_msec = msec;
  uint16_t m_rpm = rpm;
  float m_tank = tankLitters;
  int8_t m_temp = tempC;
  int8_t m_invTemp = invTemp;
  float m_dist = tripDistance;
  float m_cons = tripConsumption;
  int16_t m_sAng = steeringAngle;
  uint8_t m_numSat = numSat;
  uint8_t m_speed = speed;
  uint8_t m_ccSpeed = ccSpeed;
  int8_t m_tempRoom = tempRoomC;
  char m_voltages[128];
  strncpy(m_voltages, voltagesStr, sizeof(m_voltages));
  m_voltages[sizeof(m_voltages) - 1] = '\0';
  float m_current = battCurrent;
  uint8_t m_cl = calculatedLoad;
  uint16_t m_tripCounter = tripCounter;
  portEXIT_CRITICAL(&canReadMux);

  int n = snprintf(out, outSize,
                   "{\"rpm\":%u,\"tank\":%.1f,\"temp\":%d,\"invTemp\":%d,"
                   "\"dist\":%.2f,\"cons\":%.2f,\"msec\":%lu,\"sAng\":%d,"
                   "\"numSat\":%u,\"speed\":%u,\"ccSpeed\":%u,\"tripCounter\":%u,"
                   "\"tempRoomC\":%d,\"battV\":\"%s\",\"current\":%.1f,\"cl\":%u}",
                   m_rpm, m_tank, m_temp, m_invTemp,
                   m_dist, m_cons, m_msec, m_sAng,
                   m_numSat, m_speed, m_ccSpeed, m_tripCounter,
                   m_tempRoom, m_voltages, m_current, m_cl);

  if (n < 0) return -1;
  if ((size_t)n >= outSize) return (int)(outSize - 1);  // truncated
  return n;
}

void readSettings() {
  if (SD.exists("/Settings.json")) {
    File Settings = SD.open("/Settings.json", FILE_READ);
    if (!Settings) {
      Serial.println(F("Failed to open Settings file for reading"));
      return;
    }

    size_t fileSize = Settings.size();
    String jsonString;
    jsonString.reserve(fileSize + 1);
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

  File file = SD.open("/GPSdata.tsv", FILE_APPEND);
  if (file) {
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
    //vTaskDelay(100 / portTICK_RATE_MS);
  }
}


void connect2WIFI() {
  static bool wiFiConnectied = false;
  if (WiFi.status() == WL_CONNECTED || WIFI_SSID == "") {  //&& mDNSInitialized) {
    if (!wiFiConnectied) {
      wiFiConnectied = true;
      Serial.print(F("RRSI: "));
      Serial.println(WiFi.RSSI());
      Serial.print(F(" CONNECTED: "));
      Serial.println(WiFi.localIP());
      configTime(0, 0, "0.europe.pool.ntp.org", "rs.pool.ntp.org", "pool.ntp.org");
    }
    return;
  } else {
    WiFi.disconnect(true);  // Clear any stale connection
    wiFiConnectied = false;
    vTaskDelay(100 / portTICK_RATE_MS);
    WiFi.setHostname(HOSTNAME);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (WiFi.waitForConnectResult(3000) != WL_CONNECTED) {
      Serial.print(F("WIFI connecting to "));
      Serial.println(WIFI_SSID);
    }
  }
}

bool waitForDoorOrTimeout() {
  int sleepCheckCounter = 0;

  // Loop for a maximum of 30 seconds (300 iterations)
  while (sleepCheckCounter < 300) {

    if (digitalRead(IGNin) == HIGH) return false;  //car started again

    if (flashedOnce == true) return true;  // door opened, Go to sleep immediately
    vTaskDelay(100 / portTICK_RATE_MS);
    sleepCheckCounter++;
    /*     if (sleepCheckCounter % 50 == 0) {
      Serial.printf("Sleep check progress: %d of %d\n", sleepCheckCounter, 300);
    } */
  }
  Serial.println(F("30-second timeout reached. Proceeding to sleep."));
  return true;  // RETURN TRUE: Go to sleep
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
      if (waitForDoorOrTimeout()) go2Sleep();
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
      uint8_t rowCount = 0;    // Reset row count for each batch
      uint8_t row = liveData;  // Reset row for each batch

      char line[256];     // TSV linija - sadrzi do 13 polja
      char fieldBuf[32];  // Pojedinacno polje iz linije
      char keyBuf[32];    // JSON put-key "values/[col]/[row]"

      while (rowCount < 50 && dataFile.available()) {
        size_t lineLen = dataFile.readBytesUntil('\r', line, sizeof(line) - 1);
        line[lineLen] = '\0';
        if (dataFile.peek() == '\n') dataFile.read();

        if (lineLen < 13) { break; }  // Only process non-empty lines

        uint8_t col = 0;
        size_t startIdx = 0;

        for (size_t i = 0; i <= lineLen && col < 13; i++) {
          if (line[i] == '\t' || line[i] == '\0') {
            size_t fieldLen = i - startIdx;
            if (fieldLen >= sizeof(fieldBuf)) fieldLen = sizeof(fieldBuf) - 1;
            memcpy(fieldBuf, line + startIdx, fieldLen);
            fieldBuf[fieldLen] = '\0';

            // change decimal separator for non US-style Google sheet
            if (col != 4) {
              for (size_t j = 0; j < fieldLen; j++) {
                if (fieldBuf[j] == '.') fieldBuf[j] = ',';
              }
            }

            snprintf(keyBuf, sizeof(keyBuf), "values/[%u]/[%u]", col, row);
            valueRange.set(keyBuf, fieldBuf);

            startIdx = i + 1;
            col++;
          }
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
    if (waitForDoorOrTimeout()) go2Sleep();
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
  snprintf(out, outSize, "%02d.%02d.%04d %02d:%02d:%02d",
           date.day(), date.month(), date.year(),
           time.hour(), time.minute(), time.second());
}
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".svg")) return "image/svg+xml";
  if (filename.endsWith(".woff2")) return "font/woff2";
  return "text/plain";  // Default
}

// Handler for serving static files with GZIP support
void handleStaticFile(AsyncWebServerRequest* request) {
  String path = request->url();
  if (path.equals("/")) {
    path = "/index.html";
  }

  String contentType = getContentType(path);

  // --- GZIP Logic ---
  bool canGzip = request->hasHeader("Accept-Encoding") && request->header("Accept-Encoding").indexOf("gzip") != -1;
  String pathWithGz = path + ".gz";
  if (canGzip && SD.exists(pathWithGz)) {
    AsyncWebServerResponse* response = request->beginResponse(SD, pathWithGz, contentType);
    response->addHeader("Content-Encoding", "gzip");
    //response->setCacheControl("max-age=28800").setLastModified("Mon, 01 Jan 2024 00:00:00 GMT");
    request->send(response);
    return;
  }
  // --- End GZIP Logic ---

  if (SD.exists(path)) {
    AsyncWebServerResponse* response = request->beginResponse(SD, path, contentType);

    request->send(response);
    return;
  }

  request->send(404, "text/plain", "404: Not Found");
}

void webGps(void* parameter) {
  connect2WIFI();
  static uint32_t sse_id = 0;
  unsigned long lastSendTime = 0;
  const unsigned long sendInterval = 400;  //  400ms

  server.onNotFound(handleStaticFile);

  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest* request) {
    static char jsonBuf[512];
    int n = getCanReadings(jsonBuf, sizeof(jsonBuf));
    if (n > 0) {
      request->send(200, "application/json", jsonBuf);
    } else {
      request->send(500, "text/plain", "readings error");
    }
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Last ID: %u\n", client->lastId());
    }
    client->send("hello!", NULL, sse_id, 10000);
  });

  server.addHandler(&events);
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest* request) {
    static char buf[2048];
    size_t len = 0;

    // HTML preamble
    len += snprintf(buf + len, sizeof(buf) - len,
                    "<!DOCTYPE html><html><head>"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                    "<style>"
                    "body{background:#141414;color:#91E8CE;margin:8px;font-family:monospace}"
                    "pre{white-space:pre-wrap;font-size:14px;margin:0}"
                    "</style></head><body><pre>");
    // --- HEAP ---
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== HEAP ===\n"
                    "Free:      %u B\n"
                    "Min ever:  %u B\n\n",
                    ESP.getFreeHeap(), ESP.getMinFreeHeap());

    // --- RESET REASON ---
    const char* resetStr = "unknown";
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON: resetStr = "power-on"; break;
      case ESP_RST_EXT: resetStr = "external pin"; break;
      case ESP_RST_SW: resetStr = "software reset"; break;
      case ESP_RST_PANIC: resetStr = "panic (exception)"; break;
      case ESP_RST_INT_WDT: resetStr = "interrupt watchdog"; break;
      case ESP_RST_TASK_WDT: resetStr = "task watchdog"; break;
      case ESP_RST_WDT: resetStr = "other watchdog"; break;
      case ESP_RST_DEEPSLEEP: resetStr = "wake from deep sleep"; break;
      case ESP_RST_BROWNOUT: resetStr = "brownout (DC power)"; break;
      case ESP_RST_SDIO: resetStr = "SDIO"; break;
      default: break;
    }
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== RESET REASON ===\n%s\n\n", resetStr);

    // --- UPTIME ---
    unsigned long uptimeSec = millis() / 1000UL;
    unsigned long uHours = uptimeSec / 3600UL;
    unsigned long uMinutes = (uptimeSec % 3600UL) / 60UL;
    unsigned long uSeconds = uptimeSec % 60UL;
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== UPTIME ===\n%luh %lum %lus\n\n",
                    uHours, uMinutes, uSeconds);

    // --- CAR STATUS (hex) ---
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== CAR STATUS ===\n"
                    "FSS (fuel cut): %u\n"
                    "Door status:    0x%02X  (0x1F = all unlocked)\n"
                    "Windows status: 0x%02X  (RL RR FR FL, 2 bit per window)\n\n",
                    fss, door_status, windows_status);

    // --- WIFI ---
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== WIFI ===\n"
                    "SSID:   %s\n"
                    "RSSI:   %d dBm\n"
                    "IP:     %s\n\n",
                    WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());

    // --- SSE CLIENTS ---
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== SSE CLIENTS ===\n%u\n\n", events.count());

    // --- GPSdata.tsv ---
    len += snprintf(buf + len, sizeof(buf) - len, "=== GPSdata.tsv ===\n");
    if (SD.exists("/GPSdata.tsv")) {
      File f = SD.open("/GPSdata.tsv", FILE_READ);
      if (f) {
        len += snprintf(buf + len, sizeof(buf) - len,
                        "Exists: yes\nSize:   %u B\n\n", f.size());
        f.close();
      } else {
        len += snprintf(buf + len, sizeof(buf) - len,
                        "Exists: yes (cannot open file)\n\n");
      }
    } else {
      len += snprintf(buf + len, sizeof(buf) - len, "Exists: no\n\n");
    }

    // --- UPLOAD TIMING ---
    len += snprintf(buf + len, sizeof(buf) - len, "=== UPLOAD ===\n");
    long initialUploadms = 10000 + (long)uploadInterval * -1;
    if (uploadms == initialUploadms) {
      len += snprintf(buf + len, sizeof(buf) - len,
                      "Last try: never\n");
    } else {
      unsigned long elapsed = (millis() - (unsigned long)uploadms) / 1000UL;
      len += snprintf(buf + len, sizeof(buf) - len,
                      "Last try: before %lu s\n", elapsed);
    }
    long remaining = (long)uploadInterval - ((long)millis() - uploadms);
    if (remaining < 0) remaining = 0;
    len += snprintf(buf + len, sizeof(buf) - len,
                    "Next for:        %ld s (or at +%u km on odometer)\n"
                    "Uploading now:     %s\n\n",
                    remaining / 1000L, upOdomInterval, uploading ? "yes" : "no");

    // --- BLOCK INTERNAL RESISTANCE ---
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== BLOCK INTERNAL RES (mOhm) ===\n");
    for (uint8_t i = 0; i < 14; i++) {
      len += snprintf(buf + len, sizeof(buf) - len,
                      "%2u: %3u   ", i + 1, blockInternalRes[i]);
      if ((i + 1) % 4 == 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "\n");
      }
    }
    if (14 % 4 != 0) {
      len += snprintf(buf + len, sizeof(buf) - len, "\n");
    }

    len += snprintf(buf + len, sizeof(buf) - len, "</pre></body></html>");

    request->send(200, "text/html", buf);
  });

  server.begin();
  if (!sse_id) Serial.println(F(" - Web loop start -"));

  while (true) {

    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    if (gps.location.isUpdated()) {
      char tmp[20];
      formatDateTimeBuf(tmp, sizeof tmp, gps.date, gps.time);
      portENTER_CRITICAL(&canReadMux);
      strncpy(dateTime, tmp, sizeof(dateTime));
      dateTime[sizeof(dateTime) - 1] = '\0';
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      altitude = gps.altitude.meters();
      numSat = gps.satellites.value();
      portEXIT_CRITICAL(&canReadMux);
    }

    bool processVoltages = false;
    uint8_t localVoltages[MAX_ISO_TP_MSG_LEN];

    portENTER_CRITICAL(&canReadMux);
    if (voltageDataReady) {
      memcpy(localVoltages, completedVoltagesBuffer, sizeof(localVoltages));
      voltageDataReady = false;
      processVoltages = true;
    }
    portEXIT_CRITICAL(&canReadMux);

    if (processVoltages) {
      char tempVoltagesStr[128];  // 64
      size_t len = 0;
      tempVoltagesStr[0] = '\0';

      for (uint8_t i = 0; i < 14; i++) {
        uint16_t rawV = ((uint16_t)localVoltages[2 + (2 * i)] << 8) | localVoltages[3 + (2 * i)];
        float measuredV = (rawV * 79.99f / 65535.0f);
        float blockIR = (float)blockInternalRes[i] / 1000.0f;
        float staticV = measuredV + (battCurrent * blockIR);
        uint16_t staticVX10 = (uint16_t)(staticV * 10.0f + 0.5f);
        len += snprintf(tempVoltagesStr + len, sizeof(tempVoltagesStr) - len, "%u,", staticVX10);
      }

      uint16_t rawV_aux = ((uint16_t)localVoltages[30] << 8) | localVoltages[31];
      float measuredV_aux = (((float)rawV_aux * 79.9f) / 65535.0f) - 40.0f;
      uint16_t auxVX10 = (uint16_t)(measuredV_aux * 10.0f + 0.5f);
      snprintf(tempVoltagesStr + len, sizeof(tempVoltagesStr) - len, "%u", auxVX10);

      portENTER_CRITICAL(&canReadMux);
      strncpy(voltagesStr, tempVoltagesStr, sizeof(voltagesStr) - 1);
      voltagesStr[sizeof(voltagesStr) - 1] = '\0';
      portEXIT_CRITICAL(&canReadMux);
    }

    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= sendInterval) {
      if (events.count() > 0) {
        sse_id++;
        static char payload[512];
        int n = getCanReadings(payload, sizeof(payload));
        if (n > 0) {
          events.send(payload, "new_readings", sse_id);
        }
      }
      lastSendTime = currentMillis;
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);

    connect2WIFI();
    if (digitalRead(IGNin) == LOW && msec + 90000 < millis()) go2Sleep();
  }
}
