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
//#include "mdns.h"  //it doesn't work as expected so hostname does not need to be settled for bonjour browsing. I'll use my app for browsing using IP address
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "driver/rtc_io.h"

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
String WIFI_PASSWORD;
String PROJECT_ID;
String CLIENT_EMAIL;
char PRIVATE_KEY[2048];
//char HOSTNAME[32];
String spreadsheetId;
String cell;  //Spreadsheet name and first table cell
//unsigned long epochTime;
//unsigned long millisOnEpoch;
String dateTime = "0";
double latitude;
double longitude;
float altitude;
//bool mDNSInitialized = false;
bool uploading = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events");
// Json Variable to Hold Sensor Readings
FirebaseJsonData readings;
// Token Callback function
void tokenStatusCallback(TokenInfo info);
void lockDoors();  // Function declaration

// UBX Power Management Messages (more reliable for u-blox)
/* const byte UBX_POWER_DOWN[] = {
  0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x10, 0x68
}; */
// Power Down until wakeup
/* const byte UBX_PSM[] = {
  0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92
};  */
// Cyclic Power Save Mode (1Hz)

// Send UBX binary command
/* void sendUBX(const byte* message, int len) {
  gpsSerial.write(0xFF);  // Sync
  for (int i = 0; i < len; i++) {
    gpsSerial.write(message[i]);
  }
}
 */
// Function that gets current epoch time in 3 attempts
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  uint8_t step = 0;
  while (!getLocalTime(&timeinfo) && step <= 3) {
    step++;
    vTaskDelay(50 / portTICK_RATE_MS);
    Serial.print(F(" step: "));
    Serial.println(step);
  }
  time(&now);
  return now;
}



String getCanReadings() {
  static FirebaseJson json;
  json.clear();
  //json.set("odometer", odometer);
  json.set("rpm", rpm);
  json.set("tank", tankLitters);
  json.set("temp", tempC);
  json.set("dist", tripDistance);
  json.set("cons", tripConsumption);
  json.set("msec", msec);
  json.set("sAng", steeringAngle);
  json.set("numSat", numSat);
  json.set("speed", speed);
  json.set("tempRoomC", tempRoomC);
  json.set("tripCounter", tripCounter);
  //json.set("vrata", lockedDoorsIn);
  String jsonString;
  if (json.toString(jsonString)) {
    //Serial.println(jsonString);

    return jsonString;
  }
  return "{}";  // Return empty JSON on error
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
    /*     if (json.get(jsonData, "Hostname")) tempChar = jsonData.stringValue;
    tempChar.toCharArray(HOSTNAME, sizeof(HOSTNAME)); */
    if (json.get(jsonData, "private_key")) tempChar = jsonData.stringValue;
    tempChar.replace("\\n", "\n");
    tempChar.toCharArray(PRIVATE_KEY, sizeof(PRIVATE_KEY));
    if (json.get(jsonData, "project_id")) PROJECT_ID = jsonData.stringValue;
    if (json.get(jsonData, "client_email")) CLIENT_EMAIL = jsonData.stringValue;
  } else {
    Serial.println(F("Settings.json does not exist"));
  }
}



// valueRange.set("values/[0]/[0]", getTime());
// valueRange.set("values/[1]/[0]", msec);
// valueRange.set("values/[3]/[0]", odometer);
// valueRange.set("values/[5]/[0]", tankLitters);
// valueRange.set("values/[6]/[0]", dateTime);
// valueRange.set("values/[7]/[0]", latitude);
// valueRange.set("values/[8]/[0]", longitude);
// valueRange.set("values/[9]/[0]", altitude);
// valueRange.set("values/[10]/[0]", tripCounter);
// valueRange.set("values/[11]/[0]", tripDistance);
// valueRange.set("values/[12]/[0]", tripConsumption);
// valueRange.set("values/[13]/[0]", tripDistanceEV);
// valueRange.set("values/[14]/[0]", msecEV);

/*valueRange.set("values/[0]/[0]", getTime());
  valueRange.set("values/[1]/[0]", msec);
  valueRange.set("values/[2]/[0]", odometer);
  valueRange.set("values/[3]/[0]", tankLitters);
  valueRange.set("values/[4]/[0]", dateTime);
  valueRange.set("values/[5]/[0]", latitude);
  valueRange.set("values/[6]/[0]", longitude);
  valueRange.set("values/[7]/[0]", altitude);
  valueRange.set("values/[8]/[0]", tripCounter);
  valueRange.set("values/[9]/[0]", tripDistance);
  valueRange.set("values/[10]/[0]", tripConsumption);
  valueRange.set("values/[11]/[0]", tripDistanceEV);
  valueRange.set("values/[12]/[0]", msecEV); */
//      0           1           2           3             4            5      6    7       8          9               10                11              12           13          14
//Unix Date,	Milliseconds,	Odometer Km,	Tank l,	GPS DateTime (UTC),	LAT,	LONG,	ALT,	TRIP #,	Trip distance,	Trip consumption	|Trip time,  Internet DateTime,	Av speed,	Av Consumption

void write2SD() {
  // Open the file in append mode
  File file = SD.open("/GPSdata.tsv", FILE_APPEND);
  if (file) {
    // Write the data to the file
    file.print(getTime());
    file.print("\t");
    file.print(msec);
    file.print("\t");
    file.print(odometer);
    file.print("\t");
    file.print(tankLitters);
    file.print("\t");
    file.print(dateTime);
    file.print("\t");
    file.print(latitude, 9);
    file.print("\t");
    file.print(longitude, 9);
    file.print("\t");
    file.print(altitude);
    file.print("\t");
    file.print(tripCounter);
    file.print("\t");
    file.print(tripDistance);
    file.print("\t");
    file.print(tripConsumption);
    file.print("\t");
    file.print(tripDistanceEV);
    file.print("\t");
    file.print(msecEV);
    file.println("");
    file.close();
    Serial.println(F("Data appended to file."));
    //status = "Data appended\nto file.";
  } else {
    Serial.println(F("Error opening file for append."));
  }
}

/* void updateMdns() {
  esp_err_t err = mdns_init();
  if (err) {
    printf("MDNS Init failed: %d\n", err);
    return;
  }
  mdns_hostname_set(HOSTNAME);
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  mdns_service_instance_name_set("_http", "_tcp", "Prius ESP32 Can reader");
  mDNSInitialized = true;
} */

void connect2WIFI() {

  if (WiFi.status() == WL_CONNECTED || WIFI_SSID == "") {  //&& mDNSInitialized) {
    // if (epochTime > 0) epochTime += (int)((msec - millisOnEpoch) / 1000);  //update known epoch time with msec
    return;
  } else {
    //mDNSInitialized = false;
    if (WiFi.status() != WL_CONNECTED) {
      // WiFi.setHostname(HOSTNAME);
      WiFi.setHostname("Prius CanReader");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    //uint8_t step = 0;
    Serial.print(F("WIFI connecting to "));
    Serial.print(WIFI_SSID);
    Serial.print(F("\nESP32 IP Address: "));
    Serial.println(WiFi.localIP());
    //Serial.print(F("ESP32 HostName: "));
    //Serial.println(WiFi.getHostname());
    Serial.print(F("RRSI: "));
    Serial.println(WiFi.RSSI());

    //if (epochTime == 0) {
    configTime(0, 0, "0.europe.pool.ntp.org", "rs.pool.ntp.org", "pool.ntp.org");
    //  epochTime = getTime();
    //  millisOnEpoch = msec;
    //}

    Serial.print(F(" CONNECTED: "));
    Serial.println(WiFi.localIP());
    IPAddress ip = WiFi.localIP();
    char ipAddress[20];
    sprintf(ipAddress, "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    //updateMdns();
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
  esp_deep_sleep_start();
}

void go2DeepSleep() {
  WiFi.disconnect(true);
  gpio_hold_dis (gpsOn);
  digitalWrite(gpsOn, 0);
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_34, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}

void upLoad2Google(void* parameter) {
  //if (msec > uploadms + 10000) { upLoad2Google(NULL); }  // less than 10 secondes passed after last upload //upLoadTask();

  if (uploading) { return; }
  uploading = true;
  vTaskDelay(1 / portTICK_RATE_MS);
  //status = "upload";
  connect2WIFI();
  if (WiFi.status() != WL_CONNECTED) {
    write2SD();
    uploading = false;
    if (digitalRead(IGNin) == LOW) {
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

  // Set live data first
  valueRange.set("values/[0]/[0]", getTime());
  valueRange.set("values/[1]/[0]", msec);
  valueRange.set("values/[2]/[0]", odometer);
  valueRange.set("values/[3]/[0]", tankLitters);
  valueRange.set("values/[4]/[0]", dateTime);
  valueRange.set("values/[5]/[0]", latitude);
  valueRange.set("values/[6]/[0]", longitude);
  valueRange.set("values/[7]/[0]", altitude);
  valueRange.set("values/[8]/[0]", tripCounter);
  valueRange.set("values/[9]/[0]", tripDistance);
  valueRange.set("values/[10]/[0]", tripConsumption);
  valueRange.set("values/[11]/[0]", tripDistanceEV);
  valueRange.set("values/[12]/[0]", msecEV);
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
      valueRange.clear();
      SD.remove("/GPSdata.tsv");
      if (!SD.exists("/GPSdata.tsv")) Serial.println(F("File /GPSdata.tsv deleted successfully"));
    }
  } else {
    Serial.println(GSheet.errorReason());
    write2SD();
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

String formatDateTime(TinyGPSDate date, TinyGPSTime time) {
  char dateTime[20];
  sprintf(dateTime, "%02d.%02d.%04d %02d:%02d:%02d",
          date.day(), date.month(), date.year(),
          time.hour(), time.minute(), time.second());
  return String(dateTime);
}

//bool posInfo = true;

void webGps(void* parameter) {

  Serial.print(F("webGps = "));
  Serial.println(gpsSerial.available());
  connect2WIFI();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SD, "/index.html", "text/html");
  });
  server.serveStatic("/", SD, "/");
  // Request for the latest data
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = getCanReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Last ID: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, msec, 10000);
  });
  server.addHandler(&events);

  // Start server
  server.begin();


  while (true) {
    events.send(getCanReadings().c_str(), "new_readings", msec);
    while (gpsSerial.available() > 0) {  //&& getGPS
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isUpdated()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      altitude = gps.altitude.meters();
      dateTime = formatDateTime(gps.date, gps.time);
      numSat = gps.satellites.value();
    }

    vTaskDelay(450 / portTICK_RATE_MS);
    connect2WIFI();
    /*     if (digitalRead(IGNin) == LOW) {
      go2Sleep();
    } */
  }
}
