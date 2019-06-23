/*
 * <copyright>
 * Copyright (c) 2017: Tuomas Huuki
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

#ifdef ETHERNET_ENABLED
#include <Ethernet2.h>
// Ethernet stuff.
byte mac[] = { 0x98, 0x76, 0xB6, 0x10, 0x57, 0x75 };
IPAddress ip(192, 168, 1, 60);
IPAddress dnsServer(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(80);

#define WIZ_CS 10
#endif

// Physical pins for radio.
#define RFM95_CS      8
#define RFM95_INT     3
#define RFM95_RST     4

// LED pin.
#define LED 13

// Radio frequency.
#define RF95_FREQ 433.0

// Server end address.
#define MAX_ADDRESSES 16
#define SERVER_ADDRESS 1

// Radio driver and package manager.
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, SERVER_ADDRESS);

#define MAX_TEMP_SENSORS 5

// Protocol is simple:
// Sync bytes $$$$$
// Amount of data to expect minus the lenght byte. (uint8_t)
// Source address. (0 for server) (uint8_t)
// RSSI (0 for server) (int8_t)
// Data type (uint32_t)
// Data..

// Commands.
#define dataStructVersion 1
#define deviceVersion 0x76
#define invalidRequest 0xF0

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
  float windSpeedAvg;
  float windSpeedMax;
  float windDirection;
  float windDirectionVariance;
  float waveHeight;
}measStruct;

#define SERIALBUFFERLENGTH 512

const uint8_t softwareVersionMajor = 0;
const uint8_t softwareVersionMinor = 0;
const uint8_t softwareVersionBuild = 10;

uint8_t receptionBuffer[RH_RF95_MAX_MESSAGE_LEN];
uint8_t serialBuffer[SERIALBUFFERLENGTH];
uint8_t receivedBytes = RH_RF95_MAX_MESSAGE_LEN;
uint8_t sourceAddress;

measStruct lastData[MAX_ADDRESSES];
int8_t rssi[MAX_ADDRESSES];  

#ifdef ETHERNET_ENABLED
EthernetClient client;
#endif

void setup() 
{
  // IO configuration for board.
  pinMode(LED, OUTPUT);     
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  // Init serial.
  // while (!Serial);
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
      Serial.print("ERROR: Cannot init package management.");  
      delay(1000);
    }
  }
  
  manager.setTimeout(5000);
  
#ifdef ETHERNET_ENABLED
  Serial.print("Initializing Ethernet.");
  Ethernet.init(WIZ_CS);
  Ethernet.begin(mac, ip, dnsServer, gateway, subnet);
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  server.begin();
#endif

  for(int i = 0; i < MAX_ADDRESSES; i++){
    memset(&lastData[i], 0, sizeof(measStruct));
  }
  
  Serial.println("INFO: Initialization ok, running.");
}

void loop()
{

  if(manager.available()){
    if(manager.recvfromAck(receptionBuffer, &receivedBytes, &sourceAddress)){
      if(sourceAddress < MAX_ADDRESSES){
        rssi[sourceAddress] = rf95.lastRssi();
        Serial.write("$$$$$");
        Serial.write(sizeof(measStruct) + 2);
        Serial.write(sourceAddress);
        Serial.write(rssi[sourceAddress]);
        memcpy((uint8_t *)&lastData[sourceAddress], receptionBuffer, receivedBytes);
        Serial.write((uint8_t*)&lastData[sourceAddress], sizeof(measStruct));
      }
    }
  }

  if(Serial.available()){
    uint8_t bytes = Serial.readBytesUntil('\n', serialBuffer, SERIALBUFFERLENGTH);  
    if(bytes != 0){
      switch (serialBuffer[0]){
        case deviceVersion:
        {
          uint8_t packetLength = sizeof(uint32_t) + 5 * sizeof(uint8_t);
          uint32_t responseType = deviceVersion;
          Serial.write("$$$$$");
          Serial.write(packetLength);
          Serial.write((uint8_t) 0); // id
          Serial.write((uint8_t) 0); // rssi
          Serial.write((uint8_t *)&responseType, sizeof(uint32_t));
          Serial.write(softwareVersionMajor);
          Serial.write(softwareVersionMinor);
          Serial.write(softwareVersionBuild);
        }
          break;
        
        default:
          uint8_t packetLength = sizeof(uint8_t);
          uint8_t responseType = invalidRequest;
          Serial.write("$$$$$");
          Serial.write(packetLength);
          Serial.write(responseType);
          break;
      }
    }
  }

#ifdef ETHERNET_ENABLED
  rf95.process();
  client = server.available();  
  // listen for incoming clients
  if (client) {
      boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int i = 0; i < MAX_ADDRESSES; i++) {
            client.println(i);
            client.println(rssi[i]);
            client.println(lastData[i].dataVersion);
            client.println(lastData[i].battmV);
            client.println(lastData[i].airTemp);
            client.println(lastData[i].airPressureHpa);
            client.println(lastData[i].airHumidity);
            client.println(lastData[i].sensorCount);
            for(int j = 0; j < lastData[i].sensorCount; j++){
              client.println(lastData[i].tempArray[j]);
            }
            client.println(lastData[i].windSpeedAvg);
            client.println(lastData[i].windSpeedMax);
            client.println(lastData[i].windDirection);
            client.println(lastData[i].windDirectionVariance);
            client.println(lastData[i].waveHeight);
            client.println("<br>");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }

    delay(1);
    client.stop();
  }
  #endif
  
}
