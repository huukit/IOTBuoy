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

#include <SparkFun_VL53L1X_Arduino_Library.h>

#include <RTCZero.h>
#include <Adafruit_SleepyDog.h>

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
#define CLIENT_ADDRESS 10

// Radio driver and package manager.
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, CLIENT_ADDRESS);

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

// VL53L1X
VL53L1X distanceSensor;
#define VL53L1X_SLEEP 9

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

typedef struct _measStruct{
  const uint32_t dataVersion = dataStructVersion;
  uint32_t loopCounter;
  uint32_t battmV;  
  float measuredvbat;
  float airTemp;
  float airPressureHpa;
  float airHumidity;
  uint32_t sensorCount;
  float tempArray[MAX_TEMP_SENSORS];
  int32_t waterHeight;
}measStruct;

uint32_t loopcounter;

static const uint32_t wTableLength = 4;
typedef struct _wTable{
  uint32_t x[wTableLength];
  int32_t y[wTableLength];
  uint8_t axislen;
}wTable;

wTable waterLookup = {.x = {50, 80, 110, 150}, .y = {-500, 0, 500, 1000}, .axislen = wTableLength};

// Get water height from sensor value.
int32_t getWaterHeight(uint32_t sensorval){
   
  float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  uint32_t Qx1 = 0, Qx2 = 0;
  float xvalue = sensorval;
    
  // If we are lower than the scale.
  if(xvalue <= waterLookup.x[0]){
    return waterLookup.y[0];
  }

  // Over the scale.    
  if(xvalue >= waterLookup.x[waterLookup.axislen - 1]){
    return waterLookup.y[waterLookup.axislen - 1];
  }
    
  // Find the correct x1,2 and y1,2
  for(uint32_t i = 0; i <waterLookup.axislen; i++){
    if((xvalue >= waterLookup.x[i] && xvalue < waterLookup.x[i + 1]) || xvalue < waterLookup.x[0]){
      x0 = waterLookup.x[i];
      Qx1 = i;
      x1 = waterLookup.x[i + 1];
      Qx2 = i+1;
      break;
    }
  }

  y0 = waterLookup.y[Qx1];
  y1 = waterLookup.y[Qx2];

  // If line is straight.
  if(y0 == y1){
    return y0;
  }
  
  return y0 + (xvalue - x0) * ((y1 - y0)/(x1 - x0));
}

void setup() 
{
  // IO configuration for board.
  pinMode(LED, OUTPUT);     
  pinMode(RFM95_RST, OUTPUT);

  pinMode(DS_GND, OUTPUT);

  pinMode(BME280_GND, OUTPUT);
  pinMode(BME280_VCC, OUTPUT);
  
  pinMode(VL53L1X_SLEEP, OUTPUT);
  
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
}

char transmissionBuffer[256];
uint8_t bufLength = 0;
bool sendOk;
float measuredvbat; 
measStruct measurements;
  
void loop()
{  
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

  // Read water height (if available);
  digitalWrite(VL53L1X_SLEEP, HIGH);
  // Get waterheight if available.
  if(distanceSensor.begin()){
    distanceSensor.setDistanceMode(2);
    //Poll for completion of measurement. Takes 40-50ms.
    while (distanceSensor.newDataReady() == false)
      delay(5);

    int distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
    measurements.waterHeight = getWaterHeight(distance);
  }
  digitalWrite(VL53L1X_SLEEP, LOW);
  
  Watchdog.reset();
  Watchdog.disable();

  sendOk = manager.sendtoWait((uint8_t *)&measurements, sizeof(measurements), SERVER_ADDRESS);
  if(!sendOk){
    Serial.println("ERROR: Did not get ACK for message");   
  }
  
  rf95.sleep();
  delay(100);
  digitalWrite(LED, LOW);
  rtc.standbyMode();    // Sleep until next alarm match
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
