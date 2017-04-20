/*
 * <copyright>
 * Copyright (c) 2016: Tuomas Huuki
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
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
 * Server end for IOT weather buoy. (license: GPLv2 or LGPLv2.1)
 * \author Tuomas Huuki tuomas.huuki@proximia.fi
 * \copyright 2017 Tuomas Huuki / proximia.fi
 */

#include <SPI.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>

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

// Radio driver and package manager.
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, SERVER_ADDRESS);

#define MAX_TEMP_SENSORS 5

#define DATASTRUCT_TYPE 1

// Structure with the measurements.
typedef struct _measStruct{
  uint32_t battmV;  
  float measuredvbat;
  float airTemp;
  float airPressureHpa;
  float airHumidity;
  uint32_t sensorCount;
  float tempArray[MAX_TEMP_SENSORS];
}measStruct;

void setup() 
{
  // IO configuration for board.
  pinMode(LED, OUTPUT);     
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  // Init serial.
  while (!Serial);
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
  rf95.setTxPower(5, false);

  if(!manager.init()){
    while(1){
      Serial.print("ERROR: Cannot init package management.");  
      delay(1000);
    }
  }
  Serial.print("INFO: Initialization ok, running.");
}

void loop()
{
  uint8_t receptionBuffer[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t bufLength = sizeof(measStruct);
  uint8_t sourceAddress;
  int8_t rssi = 0;
  measStruct measurements;
  
  if(manager.available()){
    if(manager.recvfromAck((uint8_t *)&measurements, &bufLength, &sourceAddress)){
      Serial.write("$$$$$");
      Serial.write(bufLength + 2);
      Serial.write(sourceAddress);
      rssi = rf95.lastRssi();
      Serial.write(rssi);
      Serial.write((uint8_t *)&measurements, bufLength);
      /*
      Serial.print("got request from : 0x");
      Serial.print(sourceAddress, HEX);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC); 
      Serial.println("Measurements:");
      
      Serial.print("Battery Voltage (mV): ");
      Serial.println(measurements.battmV);
      
      Serial.print("Air temp (C): ");
      Serial.println(measurements.airTemp);
      
      Serial.print("Air Pressure (hPa): ");
      Serial.println(measurements.airPressureHpa);
      
      Serial.print("Air humidity (%): ");
      Serial.println(measurements.airHumidity);

      Serial.println("Water temp array (id : C):");
      for(int i = 0; i < measurements.sensorCount; i++){
        Serial.print(i);
        Serial.print(" : ");
        Serial.println(measurements.tempArray[i]);  
      }
      */
    }
  }
  delay(20);
  /*
  if (rf95.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len))
    {
      digitalWrite(LED, HIGH);
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.print("Got: ");
      Serial.println((char*)buf);
       Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      
      // Send a reply
      uint8_t data[] = "And hello back to you";
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      Serial.println("Sent a reply");
      digitalWrite(LED, LOW);
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
  */
}


