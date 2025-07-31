/*
  Project: Gen3-CAN-Trip-Logger
  Author: Dejan Vasić (Дејан Васић)
  Description: This device interfaces with the vehicle's CAN bus to monitor, log, and display trip data, while also enhancing convenience and safety with automation features.

  This sketch uses the following third-party libraries:
  - ESP_Google_Sheet_Client by mobizt (MIT License)
  - TinyGPSPlus by Mikal Hart (LGPL)
  - ESPAsyncWebServer (LGPL)
  - esp32_can.h by Collin Kidder (MIT Licence)

  For full license details, see the NOTICE file in the root of this repository.
*/

//https://github.com/espressif/arduino-esp32 ver 2.0.17 max (no WPA3), board Wemos LOLIN32
#include "esp32_can.h"  // https://github.com/collin80/esp32_can AND https://github.com/collin80/can_common
#include "EEPROM.h"

#define CAN_ODOMETER 0x611  //0xA6
#define CAN_SPEED 0x610
#define CAN_DIMM 0x620
#define CAN_GEAR 0x3BC
#define CAN_STEER 0x25
#define CAN_DOORS 0x638
#define CAN_KEY 0x635
#define CAN_MAIN_BODY 0x750

//#define TANK_L 45.0
#define CAN_REQST_TANK 0x7C0
#define CAN_REPLY_TANK 0x7C8
#define CANPID_TANK_LEVEL 0x29

#define CAN_ID_ICE 0x7DF  //0x7E0  //Service 01 - Show current data //0x7DF
#define CAN_REPLY_ID_ICE 0x7E8

#define CAN_ID_RTEMP 0x7C4  //Room temperature
#define CAN_REPLY_ID_RTEMP 0x7CC
#define CANPID_RTEMP 0x21

#define CANPID_FUEL_INJ 0x3C  //213C
//#define CANPID_RPM 0x0C //1C4 8	16	100/128+1/4
#define CAN_RPM 0x1C4  //8	16	100/128+1/4
#define CANPID_TEMP 0x05

#define RX_PIN 26
#define TX_PIN 25
#define IGNin GPIO_NUM_34
#define gpsOn GPIO_NUM_32

//uint16_t lockedDoorsIn;
uint32_t odometer = 0;
uint32_t uploadOdometer = 0;
uint8_t upOdomInterval = 10;                // upload on every x km
const uint32_t uploadInterval = 5 * 60000;  // upload on every x minutes
long uploadms = 0;
uint8_t speed = 0;
uint8_t previousSpeed = 0;
bool inPark = false;
uint8_t door_status = 0; //0= all locked; 0x1F (31) all unlocked
bool lockedDoorsByMe = false;
bool angleAlarm = true;
float tankLitters = 0.0;
float injectedmLitters = 0.0;
uint16_t rpm = 0;
float fuelConsumption = 0.0;
float tripConsumption = 0.0;
uint8_t tankIndex = 0;
float tank[100] = { 0 };  //last 100 readings to calculate average + 1 trip start reading
float canTankLevel = 0.0f;
uint8_t readingCount = 0;
double litersSum = 0.0;
TaskHandle_t GoogleTask = NULL;
TaskHandle_t GPStask = NULL;
uint16_t tripCounter = 0;
uint8_t dimmOut = 33;  //pin
int8_t tempC = 0;
int8_t tempRoomC = 0;
uint8_t numSat = 0;
int16_t steeringAngle = 0;
unsigned long msec = 0;
unsigned long msecBefore = 0;
unsigned long msecEV = 0;
float tripDistance = 0.0;
float tripDistanceEV = 0.0;
bool oddEven = true;
#include "Upload.h"

void unlockDoors() {
  CAN_FRAME outgoing;
  outgoing.id = CAN_MAIN_BODY;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x40;
  outgoing.data.uint8[1] = 0x05;
  outgoing.data.uint8[2] = 0x30;
  outgoing.data.uint8[3] = 0x11;
  outgoing.data.uint8[4] = 0x00;
  outgoing.data.uint8[5] = 0x40;  //unlock
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
  lockedDoorsByMe = false;
}
void lockDoors() {
  CAN_FRAME outgoing;
  outgoing.id = CAN_MAIN_BODY;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x40;
  outgoing.data.uint8[1] = 0x05;
  outgoing.data.uint8[2] = 0x30;
  outgoing.data.uint8[3] = 0x11;
  outgoing.data.uint8[4] = 0x00;
  outgoing.data.uint8[5] = 0x80;  //lock
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
  lockedDoorsByMe = true;
}

void buzzerSound() {
  CAN_FRAME outgoing;
  outgoing.id = CAN_MAIN_BODY;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x40;
  outgoing.data.uint8[1] = 0x04;
  outgoing.data.uint8[2] = 0x30;
  outgoing.data.uint8[3] = 0x14;  //14 buzzer, 06 alarm or horn
  outgoing.data.uint8[4] = 0x01;  //seconds
  outgoing.data.uint8[5] = 0x80;  //80 buzzer, 40 alarm, 20 horn
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(gpsOn, OUTPUT);
  pinMode(dimmOut, OUTPUT);
  pinMode(IGNin, INPUT_PULLDOWN);  //INPUT
  GoogleTask = NULL;
  GPStask = NULL;

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    go2DeepSleep();
    return;
  }

  digitalWrite(dimmOut, 0);
  //float input_voltage = (r1 * analogReadMilliVolts(IGNin) / r2);

  Serial.begin(115200);
  if (digitalRead(IGNin) == LOW) { go2Sleep(); }

  Serial.println(F("--- --- --- --- --- Boot--- --- --- --- ---"));
  uploadms = 10000 + uploadInterval * -1;  //Try to connect after 10 seconds


  uint8_t step = 0;
  while (!SD.begin(chipSelect) && step <= 4) {
    step++;
    vTaskDelay(200 / portTICK_RATE_MS);
  }


  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD card initialization failed!"));
    digitalWrite(LED_BUILTIN, LOW);  //LOW = on
  } else {
    for (uint8_t i = 0; i <= 4; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(gpsOn, HIGH);
      readSettings();
      gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
      //Serial.println(gpsSerial.available());
      webGpsTask();
    }
  }

  CAN0.setCANPins((gpio_num_t)RX_PIN, (gpio_num_t)TX_PIN);
  CAN0.begin(CAN_BPS_500K);
  CAN0.watchFor(CAN_SPEED);
  CAN0.watchFor(CAN_DIMM);
  CAN0.watchFor(CAN_GEAR);
  CAN0.watchFor(CAN_REPLY_TANK);
  CAN0.watchFor(CAN_REPLY_ID_ICE);
  CAN0.watchFor(CAN_ODOMETER);
  CAN0.watchFor(CAN_STEER);
  CAN0.watchFor(CAN_RPM);
  CAN0.watchFor(CAN_DOORS);
  CAN0.watchFor(CAN_REPLY_ID_RTEMP);
  //CAN0.watchFor();  // no filter

  CAN0.setCallback(0, CB_SPEED);
  CAN0.setCallback(1, CB_DIMM);
  CAN0.setCallback(2, CB_GEAR);
  CAN0.setCallback(3, CB_REPLY_TANK);
  CAN0.setCallback(4, CB_REPLY_ID_ICE);
  CAN0.setCallback(5, CB_ODOMETER);
  CAN0.setCallback(6, CB_STEER);
  CAN0.setCallback(7, CB_RPM);
  CAN0.setCallback(8, CB_DOORS);
  CAN0.setCallback(9, CB_REPLY_ID_RTEMP);
  //CAN0.setCallback(10, CB_ALL);

  EEPROM.begin(2);
  tripCounter |= EEPROM.read(0) << 8;
  tripCounter |= EEPROM.read(1);
  tripCounter++;
  EEPROM.write(0, (uint8_t)(tripCounter >> 8));
  EEPROM.write(1, (uint8_t)tripCounter);
  EEPROM.commit();

  //lockedDoors = false;
  //tripDistance = 0;
  Serial.print(tripCounter);
  Serial.println(F(". trip setup completed"));
  //requestFUELLEVEL();
}

// Callback functions:

/* void CB_ALL(CAN_FRAME* can_bus) {
  Serial.print(millis());
  Serial.print("\t");
  Serial.print(can_bus->id,HEX);
  for (int i = 0; i < can_bus->length; i++) {
    Serial.print("\t");
    if (can_bus->data.uint8[i] <= 0xF) {
      Serial.print("0");
    }
    Serial.print(can_bus->data.uint8[i],HEX);
  }
  Serial.println();
} */

void CB_SPEED(CAN_FRAME* can_bus) {
  speed = can_bus->data.uint8[2];
  if (speed > 15 && door_status != 0x0) {
    lockDoors();
  } else if ((speed < previousSpeed - 18)) {  //collision detection, 35.32 km/h ~18 km/0.5s
    unlockDoors();
  }
  previousSpeed = speed;
}

void CB_DIMM(CAN_FRAME* can_bus) {
  digitalWrite(dimmOut, bitRead(can_bus->data.uint8[4], 6));
}

void CB_GEAR(CAN_FRAME* can_bus) {
  inPark = (can_bus->data.uint8[1] == 0x20);  //0x20 -P, 0x10 -R, 0x08 -N, 0x00 -D, 0x01 -S
  if (inPark && door_status != 0x1F && lockedDoorsByMe) { unlockDoors(); }
}

void CB_REPLY_TANK(CAN_FRAME* can_bus) {
  if (can_bus->data.uint8[2] == CANPID_TANK_LEVEL) {
    canTankLevel = static_cast<float>(can_bus->data.uint8[3]);
    if (canTankLevel > 0.0f && canTankLevel < 100.0f) {  //accept only probably valid values
      if (readingCount < 100) { readingCount++; }
      litersSum += canTankLevel;
      litersSum -= tank[tankIndex];
      tank[tankIndex] = canTankLevel;
      if (readingCount == 100) {           // first 100 readings are populated
        tankLitters = litersSum / 200.0f;  //can data are twice bigger than real tank
      } else {
        tankLitters = litersSum / (2.0f * readingCount);  //average of what is in the array so far.
      }
      tankIndex = (tankIndex + 1) % 100;
    }
  }
}

void CB_REPLY_ID_ICE(CAN_FRAME* can_bus) {
  if (can_bus->data.uint8[2] == CANPID_FUEL_INJ && rpm > 0) {
    injectedmLitters = ((can_bus->data.uint8[3] << 8) | can_bus->data.uint8[4]) * 2.047 / 65535;
  }
  fuelConsumption = 0.012f * injectedmLitters * (float)rpm;  // L/h  (injectedmLitters / 1000.0) * (rpm * 60 / 2 [injects every other turn]) * 4 [cylinders] / 10 [times data]
  if (can_bus->data.uint8[2] == CANPID_TEMP) {
    tempC = can_bus->data.uint8[3] - 40;
  }
}

void CB_ODOMETER(CAN_FRAME* can_bus) {
  odometer = (can_bus->data.uint8[4] << 24) | (can_bus->data.uint8[5] << 16) | (can_bus->data.uint8[6] << 8) | can_bus->data.uint8[7];
  if (uploadOdometer == 0) { uploadOdometer = odometer; }  //first read on boot
  requestTANKLEVEL();                                      //kad mu stigne kilometraža nek traži tank
}

void CB_STEER(CAN_FRAME* can_bus) {
  steeringAngle = ((can_bus->data.uint8[0] << 8) | can_bus->data.uint8[1]);
  if (angleAlarm) {
    if ((steeringAngle > 100 && steeringAngle < 347) || (steeringAngle < 3995 && steeringAngle > 1000)) buzzerSound();
  }
  angleAlarm = false;  // only once
}

void CB_RPM(CAN_FRAME* can_bus) {
  uint16_t rpmTemp = ((can_bus->data.uint8[0] << 8) | can_bus->data.uint8[1]) * 0.78125;  //* 100/128
  if (rpmTemp < 6000) rpm = rpmTemp;
}

void CB_DOORS(CAN_FRAME* can_bus) { // 0= all locked; 31 (1F)= all unlocked
   door_status = can_bus->data.uint8[2];
}

void CB_REPLY_ID_RTEMP(CAN_FRAME* can_bus) {
  tempRoomC = can_bus->data.uint8[3] * 63.75 / 255 - 6.5;
}
// \\Callback functions:


void loop() {
  msec = millis();
  if (digitalRead(IGNin) == LOW) {
    //Serial.print(" Loop, IGNin = 0 ");

    if (door_status != 0x1F && lockedDoorsByMe) { unlockDoors(); }
    upLoad2Google(NULL);  // less than 10 secondes passed after last upload //upLoadTask();
  } else if (msec - msecBefore >= 500) {
    msecBefore = msec;
    every500ms();
  }

  if (msec - uploadms >= uploadInterval || odometer - uploadOdometer >= upOdomInterval) {  //upload data every uploadInterval sec or uploadOdometer
    //status = "upLoad Task start";
    Serial.println(F("upload to Gsheet"));
    /*     if (uploadRetr) {
      uploadms = msec + 20000 + uploadInterval * -1;  //Try to connect after 20 seconds
      uploadRetr--;
    } else { */
    uploadms = msec;
    //}
    uploadOdometer = odometer;
    //vTaskSuspend(GPStask); //?!?
    upLoadTask();
  }
}

void every500ms() {
  requestICEinfo();
  tripConsumption += (fuelConsumption / 7200.0);
  if (rpm == 0) msecEV += 500;
  //if (speed > 1) {  //probably don't need this check
  tripDistance += (float)speed / 7200.0;
  if (rpm == 0) tripDistanceEV += (float)speed / 7200.0;
  //}
}

void requestTANKLEVEL() {  // Request Fuel Tank data
  //CAN0.watchFor(CAN_REPLY_TANK);
  CAN_FRAME outgoing;
  outgoing.id = CAN_REQST_TANK;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x02;
  outgoing.data.uint8[1] = 0x21;
  outgoing.data.uint8[2] = CANPID_TANK_LEVEL;
  outgoing.data.uint8[3] = 0x00;
  outgoing.data.uint8[4] = 0x00;
  outgoing.data.uint8[5] = 0x00;
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
}

void requestICEinfo() {  // Request Engine temp and Injection Volume ml


  oddEven = !oddEven;
  CAN_FRAME outgoing;
  outgoing.id = CAN_ID_ICE;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x02;
  outgoing.data.uint8[1] = 0x21;
  outgoing.data.uint8[2] = CANPID_FUEL_INJ;
  outgoing.data.uint8[3] = 0x00;
  outgoing.data.uint8[4] = 0x00;
  outgoing.data.uint8[5] = 0x00;
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
  //msgCountSent++;
  //vTaskDelay(100 / portTICK_RATE_MS);
  //outgoing.data.uint8[1] = 0x01;
  //outgoing.data.uint8[2] = CANPID_RPM;
  //CAN0.sendFrame(outgoing);

  if (oddEven) {  //once in sec
    digitalWrite(LED_BUILTIN, HIGH);
    vTaskDelay(150 / portTICK_RATE_MS);
    outgoing.data.uint8[1] = 0x01;
    outgoing.data.uint8[2] = CANPID_TEMP;
    CAN0.sendFrame(outgoing);
  } else {
    digitalWrite(LED_BUILTIN, LOW);  //LOW = on
    requestRoomTemp();
  }
}

void requestRoomTemp() {
  CAN_FRAME outgoing;
  outgoing.id = CAN_ID_RTEMP;
  outgoing.length = 8;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  outgoing.data.uint8[0] = 0x02;
  outgoing.data.uint8[1] = 0x21;
  outgoing.data.uint8[2] = CANPID_RTEMP;
  outgoing.data.uint8[3] = 0x00;
  outgoing.data.uint8[4] = 0x00;
  outgoing.data.uint8[5] = 0x00;
  outgoing.data.uint8[6] = 0x00;
  outgoing.data.uint8[7] = 0x00;
  CAN0.sendFrame(outgoing);
}



/* void writeFile(fs::FS& fs, const char* path, const char* message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message written to file");
  } else {
    Serial.println("Write failed");
  }
  file.close();
} */


// CRC Check
/*   
uint8_t data_packet[] ={0x41,0x2,0x01,0x80};

uint8_t crc_mod(uint8_t car_array[],int datasize)
{
  int checksum=0;
  for (int i = 0; i < datasize; i++)
    {
      checksum = (checksum + car_array[i]) & 0xFF;
    }
    return (256 - checksum & 0xFF)-1;
}

void main()
{
uint8_t data_crc=crc_mod(data_packet,sizeof(data_packet));
Serial.print(data_crc,HEX);
}
 */

/* void builtinLedBlink(void* parameter) {
  while (blinkLed) {
    digitalWrite(LED_BUILTIN, LOW);
    vTaskDelay(250 / portTICK_RATE_MS);
    digitalWrite(LED_BUILTIN, HIGH);
    vTaskDelay(250 / portTICK_RATE_MS);
    LedBlinkTask = NULL;
    vTaskDelete(LedBlinkTask);
  }
} */

/* void blinkTask() {
  blinkLed = true;
  if (LedBlinkTask == NULL) {
    xTaskCreatePinnedToCore(
      builtinLedBlink,   // Function to implement the task 
      "BuiltinLedBlink", // Name of the task 
      4096,              // Stack size in words
      NULL,              // Task input parameter 
      0,                 // Priority of the task 
      &LedBlinkTask,     // Task handle. 
      0);                // Core where the task should run 
  }
}
 */


void upLoadTask() {
  if (GoogleTask == NULL) {
    xTaskCreatePinnedToCore(
      upLoad2Google, /* Function to implement the task */
      "GoogleTask",  /* Name of the task */
      12288,         /* Stack size in words, 4096 is't enough 8192, 12288, 24576 */
      NULL,          /* Task input parameter */
      0,             /* Priority of the task, anything bigger than 0 (idle) sometimes trigger the watchdog */
      &GoogleTask,   /* Task handle. */
      0);            /* Core where the task should run */
  }
}

void webGpsTask() {
  if (GPStask == NULL) {
    xTaskCreatePinnedToCore(
      webGps,    /* Function to implement the task */
      "GPStask", /* Name of the task */
      8192,      /* Stack size in words*/
      NULL,      /* Task input parameter */
      0,         /* Priority of the task */
      &GPStask,  /* Task handle. */
      0);        /* Core where the task should run */
  }
}
