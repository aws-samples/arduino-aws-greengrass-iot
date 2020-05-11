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

#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <AWSGreenGrassIoT.h>

#include <Adafruit_SGP30.h>
#include <NTPClient.h>

extern const char aws_root_ca[];
extern const char thingCA[];
extern const char thingKey[];

Adafruit_SGP30 sgp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

char WIFI_SSID[]="SSID";
char WIFI_PASSWORD[]="PASSWORD";
char AWSIOTURL[]="YYYYYYYYYYY.iot.region.amazonaws.com";
char THING[]= "thingNameX";
char TOPIC_NAME[]= "Sensors/SGP30";
const char JSONPAYLOAD[] = "{ \"SensorID\" : \"%s\", \"timestamp\": %lu, \"TVOC\": %d, \"CO2\": %d, \"H2\": %d, \"Ethanol\": %d}";
String sensorID;

AWSGreenGrassIoT * greengrass;

int status = WL_IDLE_STATUS;
int tick=0;
char payload[512];
char rcvdPayload[512];
int sgp30ReadingCounter = 0;

void publishToGreengrass(String id, int TVOC, int eCO2, int h2, int ethanol) {

    timeClient.update();

    sprintf(payload,JSONPAYLOAD,id.c_str(), timeClient.getEpochTime(), TVOC, eCO2, h2, ethanol);
    if(greengrass->publish(TOPIC_NAME,payload)) {        
      Serial.print("Publish Message:");
      Serial.println(payload);
    }
    else {
      Serial.println("Publish failed");
    }  
}


void setup() {
    Serial.begin(115200);
    
    Serial.println("\n\nPublishing SGP30 sensor values to AWS IoT via Greengrass device\n");
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
    sensorID = String(sgp.serialnumber[0], HEX) + String(sgp.serialnumber[1], HEX) + String(sgp.serialnumber[2], HEX);
    sensorID.toUpperCase();
    Serial.println("Found SGP30 serial #"+ sensorID);

    greengrass = new AWSGreenGrassIoT(AWSIOTURL, THING, aws_root_ca, thingCA, thingKey);

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
    sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
    timeClient.begin();

    if(greengrass->connectToGG() == true)
    {
        Serial.println("Connected to AWS GreenGrass");
        delay(2000);

     }
    else
    {
        Serial.println("Connection to Greengrass failed, check if Greengrass is on and connected to the WiFi");
        while(1);
    }

}



void loop() {

  if (tick >= 5)  {
    tick = 0;
     if (! sgp.IAQmeasure()) {
       Serial.println("Measurement failed");
     return;
    }

     if (! sgp.IAQmeasureRaw()) {
      Serial.println("Raw Measurement failed");
      return;
    }
 
    if (greengrass->isConnected())
      publishToGreengrass(sensorID, sgp.TVOC, sgp.eCO2, sgp.rawH2, sgp.rawEthanol); 
    else
    {
        Serial.println("Greengrass not connected");
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
