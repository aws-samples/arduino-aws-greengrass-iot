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

//Implements publish and subscribe via the local GreenGrass device.
//Assumes you have a local Greengrass core on the local network.

#include <WiFi.h>

#include <WiFiClientSecure.h>
#include <AWSGreenGrassIoT.h>

#include "DHT.h"

#include <WiFiUdp.h>      //Used for the NTP time sync client
#include <NTPClient.h>    //Time sync used to add timestamp to message

#define DHTPIN 14         // what pin we're connected to
#define DHTTYPE DHT22     // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE); // Set the connection pins for DHT sensor

int publish_frequency = 5000; //Time in ms to publish sensor data

extern const char aws_root_ca[];
extern const char thingCA[];
extern const char thingKey[];


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

char WIFI_SSID[]="XXXXXXXXXX";                                                  //Your WiFi ssid
char WIFI_PASSWORD[]="XXXXXXXXX";                                               //Your WiFi Password
char AWSIOTURL[]="XXXXXXXXXXXXXX-ats.iot.us-east-1.amazonaws.com";              //Your AWS IoT core endpoint (this can be found in under settings in the AWS IoT console)
char THING[]= "Temp_Humid_Sensor";                                              //This name must match the name of your device in AWS IoT
char TOPIC_NAME[]= "Factory/3/Device/2/Status";                                 //Topic you wish the device to publish too.
char SUB_TOPIC_NAME[]= "Factory/3/Device/2/Control";                            //Topic you wish the device to subscribe too.    

const char JSONPAYLOAD[] = "{ \"DeviceID\" : \"%s\", \"timestamp\": %lu, \"tempC\": %f, \"Humid\": %f}"; 

String sensorID;
IPAddress ip;                    // the IP address of your shield


const int RedLedPin = 27;
const int AmberLedPin = 26;
const int GreenLedPin = 25;
//const int StatusLedPin = 32;
//const int SpareLedPin = 33;


float Celcius=0;


AWSGreenGrassIoT * greengrass;

int status = WL_IDLE_STATUS;
//bool isWindowOpen = false;
int tick=0;
char payload[512];
String receivedPayload = "";
bool ConnectedToGGC = false;
bool SubscribedToGGC = false;

static void subscribeCallback (int topicLen, char *topicName, int payloadLen, char *payLoad)
{
    Serial.println("subscribeCallback");
    receivedPayload = payLoad;
    
    //check if it has the 
    if (receivedPayload.indexOf("RedLED") > 0) {
      //Serial.println("contains RedLED");
      unsigned int charIndex = 0;
      charIndex = receivedPayload.indexOf("RedLED")+8;
      char value = receivedPayload.charAt(charIndex);
            
      //Serial.print("RedLED at location:");
      //Serial.println(value);  
     
      if(value == 49){
        //turn on
        digitalWrite(RedLedPin, HIGH);   // turn the LED on (HIGH is the voltage level)
      }else{
        //turn off
        digitalWrite(RedLedPin, LOW);   // turn the LED on (HIGH is the voltage level)
      }
    }
    
    if (receivedPayload.indexOf("GreenLED") > 0) {
      //Serial.println("contains GreenLED");
      unsigned int charIndex = 0;
      charIndex = receivedPayload.indexOf("GreenLED")+10; //this is the lenght of the text
      char value = receivedPayload.charAt(charIndex);
            
      //Serial.print("GreenLED at location:");
      //Serial.println(value);  
     
      if(value == 49){
        //turn on
        digitalWrite(GreenLedPin, HIGH);   // turn the LED on (HIGH is the voltage level)
      }else{
        //turn off
        digitalWrite(GreenLedPin, LOW);   // turn the LED on (HIGH is the voltage level)
      }
    }
}

void publishToGreengrass(String id, float tempC, float Humid) {

    timeClient.update();

    sprintf(payload,JSONPAYLOAD,id.c_str(), timeClient.getEpochTime(), tempC, Humid);
    if(greengrass->publish(TOPIC_NAME,payload)) {        
      Serial.print("Publish Message:");
      Serial.println(payload);
    }
    else {
      Serial.println("Publish failed");
    }  
}


void setup() {

    
    pinMode(RedLedPin, OUTPUT);
    pinMode(AmberLedPin, OUTPUT);
    pinMode(GreenLedPin, OUTPUT);

    digitalWrite(AmberLedPin, HIGH);    // turn AmberLED on as device is not yet connected.
        
    Serial.begin(115200);               //Serial port used for status monitoring.
    
    Serial.println("\n\nDevice started\n");

    
  while (status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(WIFI_SSID);
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        delay(3000);                            //Wait before retry Wifi connection
    }

    Serial.println("Connected to local wifi");

    ip = WiFi.localIP();
    Serial.print("local ip address of this ESP32 = ");
    Serial.println(ip);

    delay(10000);                               //Delay to allow wifi connection to establish
    ConnToGGC();                                //Try to connect to local GreenGrass Core device
 
    timeClient.begin();                         //Start time client 
    dht.begin();                                //Start Sensor polling

}

void ConnToGGC(void){

    ConnectedToGGC = false;
    SubscribedToGGC = false;

     while(ConnectedToGGC == false){
      
            greengrass = new AWSGreenGrassIoT(AWSIOTURL, THING, aws_root_ca, thingCA, thingKey);

            if(greengrass->connectToGG() == true)
            {
                Serial.println("Connected to AWS GreenGrass");
                digitalWrite(AmberLedPin, LOW);                     // turn Amber led off as connected
                ConnectedToGGC = true;                              //Set flag to true    

                   delay(1000);                                    //put this in to stop subscribing twice by mistake. 
                   
                   while(SubscribedToGGC == false){
                           if(greengrass->subscribe(SUB_TOPIC_NAME,subscribeCallback) == true) {
                                Serial.print("Subscribe to topic ");
                                Serial.print(SUB_TOPIC_NAME);
                                Serial.println(" was successful");
                                SubscribedToGGC = true;
                           }
                           else{
                                Serial.print("Subscribe to topic ");
                                Serial.print(SUB_TOPIC_NAME);
                                Serial.println(" failed");
                                SubscribedToGGC = false;
                           }
                           delay(1000); //wait for retry
                   }
                
             }
            else
            {
                Serial.println("Connection to Greengrass failed, check if Greengrass core is on and connected to the WiFi");
                digitalWrite(AmberLedPin, HIGH);   // turn the LED on as connection error
            }
            delay(1000);
    }
}

void loop() {
   

    if (greengrass->isConnected()){

      // Reading temperature or humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
      float h = dht.readHumidity();
      // Read temperature as Celsius
      float t = dht.readTemperature();


      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(t)) {
        Serial.println("Failed to read from DHT sensor!");
        delay(1000);
        return;
      }

      
      publishToGreengrass(THING, t, h);


      }else{
        Serial.println("Greengrass not connected");
        digitalWrite(AmberLedPin, HIGH);   // i lost connection
        //Put in some method of restart here.
        //ConnectedToGGC = false;
        ConnToGGC();
    }

  
  
    delay(publish_frequency);

}
