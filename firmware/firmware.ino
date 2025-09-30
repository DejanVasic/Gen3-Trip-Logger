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

//constexpr uint32_t CAN_ODOMETER = 0x611;
#define CAN_ODOMETER 0x611  //0xA6
#define CAN_SPEED 0x610
#define CAN_DIMM 0x620
#define CAN_GEAR 0x3BC
#define CAN_STEER 0x25
#define CAN_DOORS 0x638
//#define CAN_KEY 0x635
#define CAN_MAIN_BODY 0x750

//#define TANK_L 45.0
#define CAN_REQST_INSTRUMENT 0x7C0
#define CAN_REPLY_INSTRUMENT 0x7C8
#define CANPID_TANK_LEVEL 0x29

#define CAN_REQST_HV_ECU 0x7E2  //Cruise control, inverter temp
#define CAN_REPLY_HV_ECU 0x7EA
#define CANPID_CC_SPEED 0x21
#define CANPID_INV_TEMP 0x75

#define CAN_REQST_ICE 0x7E0  //0x7DF (broadcast to all ECUs) or a specific 0x7E0 (Engine ECU)  //Service 01 - Show current data  Service 21 - for Toyota for other information e.g. the voltage of the traction battery in a hybrid electric vehicle (HEV)
#define CAN_REPLY_ICE 0x7E8

#define CAN_REQST_AIR_CONDITIONER 0x7C4  //Room temperature
#define CAN_REPLY_AIR_CONDITIONER 0x7CC
#define CANPID_RTEMP 0x21

#define CANPID_FUEL_INJ 0x3C  //213C
#define CANPID_FUEL_STATUS 0x03
#define CANPID_RPM 0x0C  //1C4 8	16	100/128+1/4
//#define CAN_RPM 0x1C4  //8	16	100/128+1/4
#define CANPID_TEMP 0x05

#define RX_PIN 26
#define TX_PIN 25
#define IGNin GPIO_NUM_34
#define gpsOn GPIO_NUM_32
const int buzzer = 4;
const uint8_t expectedLength = 8;
//uint16_t lockedDoorsIn;
const float TIME_FRACTION = 500.0 / 3600000.0;  // 1/7200
volatile uint32_t odometer = 0;
uint32_t uploadOdometer = 0;
const uint8_t upOdomInterval = 10;          // upload on every x km
const uint32_t uploadInterval = 5 * 60000;  // upload on every x minutes
long uploadms = 10000 + uploadInterval * -1;
volatile uint8_t speed = 0;
uint8_t previousSpeed = 0;
volatile uint8_t ccSpeed = 0;
bool inPark = false;
uint8_t door_status = 0;     //0= all locked; 0x1F (31) all unlocked
uint8_t windows_status = 0;  //RL, RR, FR, FL -> 1:"Fully Open" 2:"Closed" 3:"Partially Open"
bool lockedDoorsByMe = false;
//bool anyDoorOpened = false;
//String openedDoors = "";
bool flashedOnce = true;
bool angleAlarm = true;
volatile float tankLitters = 0.0;
//float injectedmLitters = 0.0;
volatile uint8_t fss = 0;  //Fuel Cut Status
volatile uint16_t rpm = 0;
float fuelConsumption = 0.0;
//bool fuelConsumptNew = false;
volatile float tripConsumption = 0.0;
uint8_t tankIndex = 0;
float tank[100] = { 0 };  //last 100 readings to calculate average + 1 trip start reading
float canTankLevel = 0.0f;
uint8_t readingCount = 0;
double litersSum = 0.0;
TaskHandle_t GoogleTask = NULL;
TaskHandle_t GPStask = NULL;
//TaskHandle_t errorBuzzTask = NULL;
volatile uint16_t tripCounter = 0;
uint8_t dimmOut = 33;         //pin
volatile int8_t tempC = 0;    //100
volatile int8_t invTemp = 0;  //80
volatile int8_t tempRoomC = 0;
volatile uint8_t numSat = 0;
volatile int16_t steeringAngle = 0;
volatile unsigned long msec = 0;
unsigned long msecBefore = 0;
volatile unsigned long msecEV = 0;
volatile float tripDistance = 0.0;
volatile float tripDistanceEV = 0.0;
extern portMUX_TYPE canReadMux;
portMUX_TYPE canReadMux = portMUX_INITIALIZER_UNLOCKED;

//void errorBuzzer(uint8_t times = 2);
void canTemperaturesFunction();
void dummiFunction();
void requestTANKLEVEL();
void requestRPM();
void requestICEtemp();
void requestRoomTemp();
void requestInvTemp();
void requestInjectionInfo();
void requestICEfss();
void requestCCspeed();
void calculateConsumption();
#include "Upload.h"

void (*canFunctions[])() = {
  canTemperaturesFunction,
  dummiFunction,
  dummiFunction,
  dummiFunction,
  requestRPM,
  requestInjectionInfo,
  requestICEfss,
  requestCCspeed,
  dummiFunction,
  calculateConsumption
};
void (*canTemperatureFunctions[])() = {
  requestTANKLEVEL,
  dummiFunction,
  requestRoomTemp,
  dummiFunction,
  requestICEtemp,
  dummiFunction,
  requestInvTemp
};

const int numCanFunctions = 10;  //sizeof(canFunctions) / sizeof(canFunctions[0]);
const int numCanTempFunctions = sizeof(canTemperatureFunctions) / sizeof(canTemperatureFunctions[0]);
int currentFunctionIndex = 0;
int currentTemperatureFunctionIndex = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(gpsOn, OUTPUT);
  pinMode(dimmOut, OUTPUT);
  pinMode(IGNin, INPUT_PULLDOWN);  //INPUT
  GoogleTask = NULL;
  GPStask = NULL;

  ledcSetup(0, 2000, 8);
  ledcAttachPin(buzzer, 0);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && digitalRead(IGNin) == LOW) {
    tone(buzzer, 2000, 1000);
    go2DeepSleep();
    return;
  }

  digitalWrite(dimmOut, 0);
  //float input_voltage = (r1 * analogReadMilliVolts(IGNin) / r2);

  Serial.begin(115200);
  Serial.println(F("--- --- --- --- SERIAL START --- --- --- ---"));
  if (digitalRead(IGNin) == LOW) {
    tone(buzzer, 2000, 200);
    vTaskDelay(200 / portTICK_RATE_MS);
    go2Sleep();
  }

  Serial.println(F("--- --- --- --- --- Boot --- --- --- --- ---"));
  // = 10000 + uploadInterval * -1;  //Try to connect after 10 seconds


  uint8_t step = 0;
  while (!SD.begin(chipSelect) && step <= 4) {
    step++;
    vTaskDelay(200 / portTICK_RATE_MS);
  }


  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD card initialization failed!"));
    tone(buzzer, 2000, 100);
    vTaskDelay(150 / portTICK_RATE_MS);
    tone(buzzer, 2000, 100);
    vTaskDelay(150 / portTICK_RATE_MS);
    tone(buzzer, 2000, 100);
    digitalWrite(LED_BUILTIN, LOW);  //LOW = on
  } else {
    for (uint8_t i = 0; i <= 4; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      vTaskDelay(50 / portTICK_RATE_MS);
      digitalWrite(LED_BUILTIN, HIGH);
      vTaskDelay(50 / portTICK_RATE_MS);
      digitalWrite(gpsOn, HIGH);
      readSettings();
      gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
      //Serial.println(gpsSerial.available());
      if (!noSettings) { webGpsTask(); }
    }
  }

  CAN0.setCANPins((gpio_num_t)RX_PIN, (gpio_num_t)TX_PIN);
  CAN0.begin(CAN_BPS_500K);
  CAN0.watchFor(CAN_SPEED);
  CAN0.watchFor(CAN_DIMM);
  CAN0.watchFor(CAN_GEAR);
  CAN0.watchFor(CAN_REPLY_INSTRUMENT);
  CAN0.watchFor(CAN_REPLY_ICE);
  CAN0.watchFor(CAN_ODOMETER);
  CAN0.watchFor(CAN_STEER);
  CAN0.watchFor(CAN_DOORS);
  CAN0.watchFor(CAN_REPLY_AIR_CONDITIONER);
  CAN0.watchFor(CAN_REPLY_HV_ECU);
  //CAN0.watchFor();  // no filter

  CAN0.setCallback(0, CB_SPEED);
  CAN0.setCallback(1, CB_DIMM);
  CAN0.setCallback(2, CB_GEAR);
  CAN0.setCallback(3, CB_REPLY_INSTRUMENT);
  CAN0.setCallback(4, CB_REPLY_ICE);
  CAN0.setCallback(5, CB_ODOMETER);
  CAN0.setCallback(6, CB_STEER);
  CAN0.setCallback(7, CB_DOORS);
  CAN0.setCallback(8, CB_REPLY_AIR_CONDITIONER);
  CAN0.setCallback(9, CB_REPLY_HV_ECU);
  //CAN0.setCallback(11, CB_ALL);

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

void loop() {
  msec = millis();
  if (digitalRead(IGNin) == LOW) {
    if (((windows_status >> 0) & 0x01) || ((windows_status >> 2) & 0x01)) {  // rear window(s) open
      buzzerSound();
    }
    if (((windows_status >> 4) & 0x01) || ((windows_status >> 6) & 0x01)) {  // front window(s) open
      tone(buzzer, 2000, 100);
      vTaskDelay(150 / portTICK_RATE_MS);
      tone(buzzer, 2000, 100);
      vTaskDelay(150 / portTICK_RATE_MS);
    }
    if (door_status != 0x1F && lockedDoorsByMe) { unlockDoors(); }
    upLoad2Google(NULL);
  } else if (msec - msecBefore >= 50) {  //10 functions every 500 ms
    msecBefore = msec;
    canFunctions[currentFunctionIndex]();
    currentFunctionIndex = (currentFunctionIndex + 1) % numCanFunctions;
    //every500ms();
  }

  if (msec - uploadms >= uploadInterval || odometer - uploadOdometer >= upOdomInterval) {  //upload data every uploadInterval sec or uploadOdometer
    //status = "upLoad Task start";
    //Serial.println(F("upload to Gsheet"));
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

void canTemperaturesFunction() {
  canTemperatureFunctions[currentTemperatureFunctionIndex]();
  currentTemperatureFunctionIndex = (currentTemperatureFunctionIndex + 1) % numCanTempFunctions;
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
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_SPEED have unexpected CAN message length"));
    return;
  }
  static bool hazardLightsOnFlag = false;
  speed = can_bus->data.uint8[2];
  if (speed > 15 && door_status != 0x0) {
    lockDoors();
  } else if ((speed < previousSpeed - 18)) {  //collision detection, 35.32 km/h ~18 km/0.5s
    unlockDoors();
    hazardLightsOn(255);
  } else if ((speed < previousSpeed - 11) && !hazardLightsOnFlag) {  //hard breaking detection, 22 km/h = 11 km/0.5s
    hazardLightsOnFlag = true;
    hazardLightsOn(previousSpeed);
  } else if ((speed > previousSpeed + 1) && hazardLightsOnFlag) {
    hazardLightsOnFlag = false;
    hazardLightsOn(0);
  }
  previousSpeed = speed;
}

void CB_DIMM(CAN_FRAME* can_bus) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_DIMM have unexpected CAN message length"));
    return;
  }

  digitalWrite(dimmOut, bitRead(can_bus->data.uint8[4], 6));
  if (!flashedOnce && can_bus->data.uint8[5]) {  //can_bus->data.uint8[5] != 0 if any door is opened
    hazardLightsOn(1);
    flashedOnce = true;
  }
}

void CB_GEAR(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_GEAR have unexpected CAN message length"));
    return;
  }
  inPark = (can_bus->data.uint8[1] == 0x20);  //0x20 -P, 0x10 -R, 0x08 -N, 0x00 -D, 0x01 -S
  if (inPark && door_status != 0x1F && lockedDoorsByMe) { unlockDoors(); }
}

void CB_REPLY_INSTRUMENT(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_REPLY_INSTRUMENT have unexpected CAN message length"));
    return;
  }
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

/* void CB_RPM(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_RPM have unexpected CAN message length"));
    return;
  }
  uint16_t rpmTemp = ((can_bus->data.uint8[0] << 8) | can_bus->data.uint8[1]) * 0.78125;  //* 100/128
                                                                                          //if ((rpmTemp < rpm + 1000 && rpmTemp > rpm - 1000) || rpmTemp == 0) rpm = rpmTemp;
                                                                                          // if (rpmTemp < 500) {
                                                                                          // rpm = 0;
                                                                                          //} else if (rpmTemp < 6000) {  //rpmTemp < rpm + 1000 && rpmTemp > rpm - 1000

  if (rpmTemp < 500) {
    rpm = 0;
  } else if (rpmTemp < 6000) {
    rpm = rpmTemp;
  }
  //}
} */

void CB_REPLY_ICE(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_REPLY_ICE have unexpected CAN message length"));
    return;
  }
  if (can_bus->data.uint8[2] == CANPID_FUEL_INJ) {
    //fuelConsumptNew = true;
    fuelConsumption = 0.012f * ((can_bus->data.uint8[3] << 8) | can_bus->data.uint8[4]) * 2.047 / 65535.0 * (float)rpm;  // L/h  (injectedmLitters / 1000.0) * (rpm * 60 / 2 [injects every other turn]) * 4 [cylinders] / 10 [times data]
    //if (rpm < 1200) fuelConsumption = fuelConsumption * (float)rpm / 1200.0f;  //correction when cylinder 1 does not injected 10 times in 0.5 sec because of low rpm - less than 1200
    /*      if (rpm < 1200) fuelConsumption = fuelConsumption * (float)rpm / 1200.0f;  //correction when cylinder 1 does not injected 10 times in 0.5 sec because of low rpm - less than 1200
    } */
    /*     if (fss != 4 && rpm > 0) {                                                  
      fuelConsumption = 0.012f * injectedmLitters * (float)rpm;                  // L/h  (injectedmLitters / 1000.0) * (rpm * 60 / 2 [injects every other turn]) * 4 [cylinders] / 10 [times data]
      if (rpm < 1200) fuelConsumption = fuelConsumption * (float)rpm / 1200.0f;  //correction when cylinder 1 does not injected 10 times in 0.5 sec because of low rpm - less than 1200
    } else {
      fuelConsumption = 0.0f;
    } */
  } else if (can_bus->data.uint8[2] == CANPID_FUEL_STATUS) {
    fss = can_bus->data.uint8[3];
  } else if (can_bus->data.uint8[2] == CANPID_TEMP) {
    tempC = can_bus->data.uint8[3] - 40;
  } else if (can_bus->data.uint8[2] == CANPID_RPM) {
    rpm = ((can_bus->data.uint8[3] << 8) | can_bus->data.uint8[4]) / 4;
  }
}

void CB_ODOMETER(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_ODOMETER have unexpected CAN message length"));
    return;
  }
  odometer = (can_bus->data.uint8[4] << 24) | (can_bus->data.uint8[5] << 16) | (can_bus->data.uint8[6] << 8) | can_bus->data.uint8[7];
  if (uploadOdometer == 0) { uploadOdometer = odometer; }  //first read on boot
}

void CB_STEER(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_STEER have unexpected CAN message length"));
    return;
  }
  steeringAngle = ((can_bus->data.uint8[0] << 8) | can_bus->data.uint8[1]);
  if (angleAlarm) {
    if ((steeringAngle > 100 && steeringAngle < 347) || (steeringAngle < 3995 && steeringAngle > 1000)) buzzerSound();
  }
  angleAlarm = false;  // only once
}

void CB_DOORS(CAN_FRAME* can_bus) {  // 0= all locked; 31 (1F)= all unlocked
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_DOORS have unexpected CAN message length"));
    return;
  }
  door_status = can_bus->data.uint8[2];
  windows_status = can_bus->data.uint8[4];
}

void CB_REPLY_AIR_CONDITIONER(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_REPLY_AIR_CONDITIONER have unexpected CAN message length"));
    return;
  }
  if (can_bus->data.uint8[2] == CANPID_RTEMP) {
    float tempResult = can_bus->data.uint8[3] * 63.75 / 255.0 - 6.5;
    tempRoomC = (int8_t)tempResult;
  }
}

void CB_REPLY_HV_ECU(CAN_FRAME* can_bus) {
  if (can_bus->length != expectedLength) {
    Serial.println(F("CB_REPLY_HV_ECU have unexpected CAN message length"));
    return;
  }
  //Serial.print("-reply cruise or inverter-");

  if (can_bus->data.uint8[2] == CANPID_CC_SPEED) {
    if (bitRead(can_bus->data.uint8[6], 7) && can_bus->data.uint8[4] > 35) {  //
      ccSpeed = can_bus->data.uint8[4];
    } else {
      ccSpeed = 0;
    }
  } else if (can_bus->data.uint8[3] == CANPID_INV_TEMP) {
    invTemp = can_bus->data.uint8[7] - 40;
  }
  /*     Serial.print("HV wo CC: ");
    for (int i = 0; i < can_bus->length; i++) {
      Serial.print("\t");
      if (can_bus->data.uint8[i] <= 0xF) {
        Serial.print("0");
      }
      Serial.print(can_bus->data.uint8[i], HEX);
    }
    Serial.println();
  } */
}

// \\Callback functions:

// Helper function to send CAN frames
void sendCANFrame(uint32_t id, uint8_t len, const uint8_t* data) {
  CAN_FRAME outgoing;
  outgoing.id = id;
  outgoing.length = len;
  outgoing.extended = 0;
  outgoing.rtr = 0;
  for (int i = 0; i < len; i++) {
    outgoing.data.uint8[i] = data[i];
  }
  CAN0.sendFrame(outgoing);
}

// Generalized PID request
void requestPID(uint32_t id, uint8_t mode, uint8_t pid) {
  uint8_t data[8] = { 0x02, mode, pid, 0x00, 0x00, 0x00, 0x00, 0x00 };
  sendCANFrame(id, 8, data);
}

// Door control
void lockDoors() {
  uint8_t data[8] = { 0x40, 0x05, 0x30, 0x11, 0x00, 0x80, 0x00, 0x00 };
  sendCANFrame(CAN_MAIN_BODY, 8, data);
  lockedDoorsByMe = true;
  flashedOnce = false;
}

void unlockDoors() {
  uint8_t data[8] = { 0x40, 0x05, 0x30, 0x11, 0x00, 0x40, 0x00, 0x00 };
  sendCANFrame(CAN_MAIN_BODY, 8, data);
  lockedDoorsByMe = false;
}

// Hazard lights
void hazardLightsOn(uint8_t sec) {
  uint8_t data[8] = { 0x40, 0x04, 0x30, 0x14, sec, 0x40, 0x00, 0x00 };
  sendCANFrame(CAN_MAIN_BODY, 8, data);
}

// Buzzer
void buzzerSound() {
  //errorBuzzer(3);
  uint8_t data[8] = { 0x40, 0x04, 0x30, 0x14, 0x01, 0x80, 0x00, 0x00 };
  sendCANFrame(CAN_MAIN_BODY, 8, data);
  tone(buzzer, 2000, 2000);
}

// IGNORE THIS
/* void buzzerSoundTest() {
  tone(buzzer, 2000, 100);
  vTaskDelay(200 / portTICK_RATE_MS);
  uint8_t data[8] = { 0x21, 0x3d, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 };
  sendCANFrame(0x7B0, 8, data);
  vTaskDelay(400 / portTICK_RATE_MS);
  uint8_t data2[8] = { 0x21, 0x3d, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 };
  sendCANFrame(0x7B0, 8, data2);
} */

// Dummy function
void dummiFunction() {
  return;
}

// Consumption calculation
void calculateConsumption() {
  /*       if (msec < 600000) {
      Serial.print(" ms:");
      Serial.print(msec);
      Serial.print(" fss:");
      Serial.print(fss);
      Serial.print(" rpm:");
      Serial.print(rpm);
      Serial.print(" spd:");
      Serial.print(speed);
      Serial.print(" fc:");
      Serial.println(fuelConsumption);
    }
 */
  tripDistance += (float)speed * TIME_FRACTION;
  //if ((speed > 10 && fss == 4 && rpm > 1200) || rpm < 800) {  //deceleration ?
  if ((fss == 4 && speed > 15) || rpm == 0) {  //deceleration ?
    msecEV += 500;
    tripDistanceEV += (float)speed * TIME_FRACTION;
  } else {
    tripConsumption += fuelConsumption * TIME_FRACTION;
  }
}

// PID requests
void requestTANKLEVEL() {
  requestPID(CAN_REQST_INSTRUMENT, 0x21, CANPID_TANK_LEVEL);
}
void requestInjectionInfo() {
  //fuelConsumptNew = false;
  requestPID(CAN_REQST_ICE, 0x21, CANPID_FUEL_INJ);
}
void requestRPM() {
  requestPID(CAN_REQST_ICE, 0x01, CANPID_RPM);
}
void requestICEtemp() {
  requestPID(CAN_REQST_ICE, 0x01, CANPID_TEMP);
}
void requestICEfss() {
  requestPID(CAN_REQST_ICE, 0x01, CANPID_FUEL_STATUS);
}
void requestRoomTemp() {
  requestPID(CAN_REQST_AIR_CONDITIONER, 0x21, CANPID_RTEMP);
}
void requestCCspeed() {
  requestPID(CAN_REQST_HV_ECU, 0x21, CANPID_CC_SPEED);
}
void requestInvTemp() {
  requestPID(CAN_REQST_HV_ECU, 0x21, CANPID_INV_TEMP);
}

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

/* void errorBuzz(void* parameter) {
  uint8_t times = *((uint8_t*)parameter);
  for (uint8_t i = 0; i < times; i++) {
    tone(buzzer, 2000, 200);
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
  errorBuzzTask = NULL;  // reset handle to indicate completion
  vTaskDelete(NULL);     // delete this task
}
 
void errorBuzzer(uint8_t times) {
  if (errorBuzzTask == NULL) {
    
    static uint8_t timesParam;// We need to pass times by pointer, so let's store it in a static variable
    timesParam = times;
    xTaskCreatePinnedToCore(
      errorBuzz,      // Function to implement the task
      "errorBuzz",    // Name of the task
      2048,           // Stack size in words or bytes - verify
      &timesParam,    // Task input parameter
      1,              // Priority of the task
      &errorBuzzTask, // Task handle
      0               // Core where the task should run
    );
  }
}*/
