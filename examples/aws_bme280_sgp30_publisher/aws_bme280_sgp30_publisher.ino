/*
Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License").
 You may not use this file except in compliance with the License.
 A copy of the License is located at

     http://www.apache.org/licenses/LICENSE-2.0

 or in the "license" file accompanying this file. This file is distributed
 on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 express or implied. See the License for the specific language governing
 permissions and limitations under the License.
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <AWSGreenGrassIoT.h>

#include <Adafruit_SGP30.h>
#include <NTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

extern const char aws_root_ca[];
extern const char thingCA[];
extern const char thingKey[];

Adafruit_SGP30 sgp;
Adafruit_BME280 bme; // I2C

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

char WIFI_SSID[]="SSID";
char WIFI_PASSWORD[]="PASSWORD";
char AWSIOTURL[]="xxxxxxxxxxxxxxx-ats.iot.region.amazonaws.com";
char THING[]= "subscriber";

const char JSONPAYLOAD[] = "{ \"SensorID\" : \"%s\", \"timestamp\": %lu, \"TVOC\": %d, \"CO2\": %d, \"H2\": %d, \"Ethanol\": %d}";
const char JSONPAYLOAD2[] = "{ \"SensorID\" : \"%s\", \"timestamp\": %lu, \"Temperature\": %d, \"Pressure\": %d, \"Altitude\": %d, \"Humidity\": %d}";
String sgp30_ID;
String bme280_ID;

AWSGreenGrassIoT * greengrass;

int status = WL_IDLE_STATUS;
int tick=0;
char payload[512];
char rcvdPayload[512];
int sgp30ReadingCounter = 0;

void publishSGP30_toAWS(String id, int TVOC, int eCO2, int h2, int ethanol) {

    timeClient.update();

    sprintf(payload,JSONPAYLOAD,id.c_str(), timeClient.getEpochTime(), TVOC, eCO2, h2, ethanol);
    if(greengrass->publish("Sensors/SGP30", payload)) {        
      Serial.print("Publish SGP30 Message:");
      Serial.println(payload);
    }
    else {
      Serial.println("Publish SGP30 failed");
    }  
}

void publishBME280_toAWS(String id, int temp, int pr, int alt, int h) {

    timeClient.update();

    sprintf(payload,JSONPAYLOAD2,id.c_str(), timeClient.getEpochTime(), temp, pr, alt, h);
    if(greengrass->publish("Sensors/BME280", payload)) {        
      Serial.print("Publish BME280 Message:");
      Serial.println(payload);
    }
    else {
      Serial.println("Publish BME280 failed");
    }  
}

void setup() {
    unsigned bme280_status;
    
    Serial.begin(115200);
    
    Serial.println("\n\nPublishing SGP30 sensor values to AWS IoT core\n");
    while (status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(WIFI_SSID);
        // Connect to WPA/WPA2 network. 
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        // wait 3 seconds for connection:
        delay(3000);

    }

    Serial.println("Connected to wifi");

    if (! sgp.begin()){
      Serial.println("Sensor not found :(");
      while (1);
    }
    sgp30_ID = String(sgp.serialnumber[0], HEX) + String(sgp.serialnumber[1], HEX) + String(sgp.serialnumber[2], HEX);
    sgp30_ID.toUpperCase();
    Serial.println("Found SGP30 serial 0x"+ sgp30_ID);

    greengrass = new AWSGreenGrassIoT(AWSIOTURL, THING, aws_root_ca, thingCA, thingKey);

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
    sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!

 // BM280 initialization
    bme280_status = bme.begin();
    bme280_ID = String(bme.sensorID(), HEX);
    if (!bme280_status) {

        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme280_ID);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("        ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1);
    }
    Serial.print("Found BME280 serial 0x"); Serial.println(bme.sensorID(), 16);
         
    timeClient.begin();

    if(greengrass->connectToIoTCore() == true)
    {
        Serial.println("Connected to AWS IoT core");
        delay(2000);

     }
    else
    {
        Serial.println("Connection to AWS IoT Core failed");
        while(1);
    }

}



void loop() {
  
    if (tick >= 3)  {
      tick = 0;
      if (! sgp.IAQmeasure()) {
        Serial.println("Measurement failed");
        return;
      }

      if (! sgp.IAQmeasureRaw()) {
        Serial.println("Raw Measurement failed");
        return;
      }

    int temp      = bme.readTemperature();
    int pressure  = bme.readPressure()/100.0F;
    int altitude  = bme.readAltitude(SEALEVELPRESSURE_HPA);
    int humidity  = bme.readHumidity();
    
    if (greengrass->isConnected()) {
      publishSGP30_toAWS(sgp30_ID, sgp.TVOC, sgp.eCO2, sgp.rawH2, sgp.rawEthanol); 
      publishBME280_toAWS(bme280_ID, temp, pressure, altitude, humidity);
    }
    else {
        Serial.println("No Coonection to AWS/Greeengrass");
        while (1){}
    }
    
    sgp30ReadingCounter++;
    if (sgp30ReadingCounter == 30) {
      sgp30ReadingCounter = 0;

      uint16_t TVOC_base, eCO2_base;
      if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
        Serial.println("Failed to get baseline readings");
        return;
      }
      Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
      Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
    }
  }
  vTaskDelay(1000 / portTICK_RATE_MS); 
  tick++;
}
