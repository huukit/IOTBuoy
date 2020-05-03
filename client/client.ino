 /*
 * <copyright>
 * Copyright (c) 2016: Tuomas Huuki
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2chrome-extension://mpognobbkildjkofajifpdfhcoklimli/components/startpage/startpage.html?section=Speed-dials&activeSpeedDialIndex=0
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the (Lesser) GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * </copyright>
 */

/* $Id$ */

/*! \file
 * Client end for IOT weather buoy. (license: GPLv2 or LGPLv2.1)
 * \author Tuomas Huuki tuomas.huuki@proximia.fi
 * \copyright 2017 Tuomas Huuki / proximia.fi
 */

#include <SPI.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>

#include <OneWire.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME680.h>

#include <RTCZero.h>
#include <Adafruit_SleepyDog.h>

#include <FlashStorage.h>

// Physical pins for radio.
#define RFM95_CS      8
#define RFM95_INT     3
#define RFM95_RST     4

// LED pin.
#define LED 13

// Radio frequency.
#define RF95_FREQ 433.0

// Server end address.
#define SERVER_ADDRESS 1
#define BOOT_CLIENT_ADDRESS 2

// Radio driver and package manager.
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, BOOT_CLIENT_ADDRESS);

// Battery voltage.
#define VBATPIN A7

// DS18S20
#define DS_GND 11
#define DS_DATA 12
#define MAX_TEMP_SENSORS 5

OneWire ds(DS_DATA);

// BME280
#define BME280_VCC 6
#define BME280_GND 5

Adafruit_BME280 bme280;
Adafruit_BME680 bme680;
bool bme680status;

// RTC sleep
RTCZero rtc;
const byte seconds = 0;
const byte minutes = 00;
const byte hours = 17;
const byte day = 17;
const byte month = 11;
const byte year = 15;

// Helpers
void printValues();
uint8_t readTempArray(float * arr);

// Structure with the measurements.
#define dataStructVersion 1

typedef enum {kInitial = 0x00, kRunning = 0x01} nodeStates;

typedef struct _measStruct{
  const uint32_t dataVersion = dataStructVersion;
  uint32_t loopCounter;
  nodeStates nodeState;
  uint32_t battmV;  
  float measuredvbat;
  float airTemp;
  float airPressureHpa;
  float airHumidity;
  uint32_t sensorCount;
  float tempArray[MAX_TEMP_SENSORS];
  float windSpeedAvg;
  float windSpeedMax;
  float windDirection;
  float windDirectionVariance;
  float waveHeight;
  int32_t waterHeight;
}measStruct;

uint32_t loopcounter;
uint8_t clientAddress;

FlashStorage(addr_storage, uint8_t);

void setup() 
{
  // IO configuration for board.
  pinMode(LED, OUTPUT);     
  pinMode(RFM95_RST, OUTPUT);

  pinMode(DS_GND, OUTPUT);

  pinMode(BME280_GND, OUTPUT);
  pinMode(BME280_VCC, OUTPUT);
    
  digitalWrite(DS_GND, LOW);

  digitalWrite(RFM95_RST, HIGH);

  digitalWrite(LED, LOW);
  
  // Init serial.
  //while (!Serial);
  Serial.begin(115200);
  delay(100);

  Serial.println("INFO: IOTBuoy v0.1 (c) proximia.fi");
  
  // Reset radio.
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  // Init radio, wait in loop if something failes.
  while (!rf95.init()) {
    while(1){
      Serial.println("ERROR: Cannot initialize radio.");
      delay(1000);
    }
  }

  rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
  //rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
  
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("ERROR: Cannot set requested frequency.");
    while (1);
  }
  Serial.print("INFO: Frequency set: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  if(!manager.init()){
    while(1){
      Serial.println("ERROR: Cannot init package management.");  
      delay(1000);
    }
  }
  manager.setTimeout(5000);
 
  bme680status = bme680.begin();
  if(bme680status){
    bme680.setTemperatureOversampling(BME680_OS_8X);
    bme680.setHumidityOversampling(BME680_OS_2X);
    bme680.setPressureOversampling(BME680_OS_4X);
    bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme680.setGasHeater(320, 150); // 320*C for 150 ms
  }
  else{
    Serial.println("INFO: Cannot find BME680 sensor.");  
  }
  
  Serial.println("INFO: Initialization ok, running.");

  // Start RTC
  rtc.begin();
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);
  rtc.setAlarmSeconds(00); // RTC time to wake, currently seconds only
  rtc.enableAlarm(rtc.MATCH_SS); // Match seconds on

  loopcounter = 0;

  clientAddress = BOOT_CLIENT_ADDRESS;
  
}

char transmissionBuffer[256];
uint8_t bufLength = 0;
bool sendOk;
float measuredvbat; 
measStruct measurements;
  
void loop(){ 
  if((addr_storage.read() == 255 || addr_storage.read() == 0)){
    // Request node address allocation from server.
    measurements.nodeState = kInitial;
  }
  else{
    measurements.nodeState = kRunning;
    clientAddress = addr_storage.read();
    manager.setThisAddress(clientAddress);
  }
  digitalWrite(LED, HIGH);
  Serial.println("INFO: Sending message!");   

  measurements.loopCounter = loopcounter++;
  
  // Measure battery voltage.
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  measurements.battmV = measuredvbat * 1000;

  // Read bosch envsensor.
  digitalWrite(BME280_GND, LOW);
  digitalWrite(BME280_VCC, HIGH); 
  delay(500); 
  
  bool status;
  Watchdog.enable(2000);
  status = bme280.begin(0x76);
  
  measurements.airTemp = 0;
  measurements.airPressureHpa = 0;
  measurements.airHumidity = 0;
  
  if (!status){
    Serial.println("Could not find a valid BME280 sensor, checking for 680!");
    if(bme680status){
      if(bme680.performReading()){
        measurements.airTemp = bme680.temperature;
        measurements.airPressureHpa = bme680.pressure / 100.0F;
        measurements.airHumidity = bme680.humidity;
      }
      else{
        Serial.println("Could not read values for BME680!");
      }
    }
    else{
      Serial.println("Could not find a valid BME680 sensor, check wiring!"); 
    } 
  }
  else{
    // Get local air temp values.
    measurements.airTemp = bme280.readTemperature();
    measurements.airPressureHpa = bme280.readPressure() / 100.0F;
    measurements.airHumidity = bme280.readHumidity();
  }
  digitalWrite(BME280_VCC, LOW);   
  Watchdog.reset();
    
  // Read DS temp array.
  measurements.sensorCount = readTempArray(measurements.tempArray);
  Serial.println("Sensorscount: ");
  Serial.println(measurements.sensorCount);
  for(int i = 0; i < measurements.sensorCount; i++)
    Serial.println(measurements.tempArray[i]);
  
  Watchdog.reset();
  Watchdog.disable();

  sendOk = manager.sendtoWait((uint8_t *)&measurements, sizeof(measurements), SERVER_ADDRESS);
  if(!sendOk){
    Serial.println("ERROR: Did not get ACK for message");   
  }

  // Expect address allocation from master.
  if(measurements.nodeState == kInitial){
    uint8_t receptionBuffer[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t receivedBytes = RH_RF95_MAX_MESSAGE_LEN;
    uint8_t sourceAddress;
    uint8_t count = 0;
    bool avail = false;

    while(!avail && count < 10){
      count++;
      avail = manager.available();
      Serial.println("Waiting for address from master server.");   
      delay(500);
    }
    if(avail){
      if(manager.recvfromAck(receptionBuffer, &receivedBytes, &sourceAddress)){
        if(sourceAddress == SERVER_ADDRESS){
          clientAddress = receptionBuffer[0];
          addr_storage.write(clientAddress);
          Serial.println("Address allocation ok, binding.");   
        }
      } 
    }  
    else{
      Serial.println("No allocation, retry");   
    }
  }
  
  rf95.sleep();
  delay(1000);
  digitalWrite(LED, LOW);
  if(measurements.nodeState = kRunning)rtc.standbyMode();    // Sleep until next alarm match
}

uint8_t readTempArray(float * arr){
  uint8_t addr[8];
  uint8_t data[12];   
  uint8_t present;
  uint8_t type_s; 
  uint8_t sensorCount = 0;
  int16_t raw;
  
  // TODO: This is from the example and is shit. Rewrite.
  for(int i = 0; i < MAX_TEMP_SENSORS; i++){
    Serial.println(i);
    
    if(!ds.search(addr)){
      Serial.println("No more addresses.");
      break;
    }
    
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      break;
    }

    ds.reset();    
    ds.select(addr);   
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
    delay(1000);     // maybe 750ms is enough, maybe not

    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad

    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        Serial.println("  Chip = DS18S20");  // or old DS1820
        type_s = 1;
        break;
      case 0x28:
        Serial.println("  Chip = DS18B20");
        type_s = 0;
        break;
      case 0x22:
        Serial.println("  Chip = DS1822");
        type_s = 0;
        break;
      default:
        Serial.println("Device is not a DS18x20 family device.");
        break;
    } 

    for (int j = 0; j < 9; j++) {           // we need 9 bytes
      data[j] = ds.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    
    arr[sensorCount++] = (float)raw / 16.0; 
  } 
  ds.reset_search();
  return sensorCount;
}

void printValues() {
    Serial.print("Temperature = ");
    Serial.print(bme280.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");

    Serial.print(bme280.readPressure() / 100.0F);
    Serial.println(" hPa");
/*
    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");
*/
    Serial.print("Humidity = ");
    Serial.print(bme280.readHumidity());
    Serial.println(" %");

    Serial.println();
}
