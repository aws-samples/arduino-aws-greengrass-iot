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

#include <WiFi.h>
#include <ServoESP32.h>
#include <AWSGreenGrassIoT.h>

enum command {
  CMD_OPEN,
  CMD_CLOSE,
  CMD_UNKNOWN
};

extern const char aws_root_ca[];
extern const char thingCA[];
extern const char thingKey[];

char WIFI_SSID[]="SSID";
char WIFI_PASSWORD[]="password";
char AWSIOTURL[]="xxxxxxxxxxxxxx-ats.iot.region.amazonaws.com";
char THING[]= "thingName";
char TOPIC_NAME[]= "Window/#";
String topicClose = "Window/close";
String topicOpen  = "Window/open";
String rcvdTopic;
String rcvdPayload;
static const int servoPin = 0;

Servo servo1;
AWSGreenGrassIoT  * greengrass;

int status = WL_IDLE_STATUS;
bool isWindowOpen = false;
int msgReceived = 0;
command cmdReceived; 
char payload[512];


void windowClose() {
    if (isWindowOpen ) {

      Serial.println("rotating -90 degrees");
      for(int posDegrees = 90; posDegrees >= 0; posDegrees--) {
        servo1.write(posDegrees);
        delay(10);
      }
      isWindowOpen = false;
    }
    else {
      Serial.println("Window already closed, no action");  
    }
}

void windowOpen() {
    if (!isWindowOpen) {
      Serial.println("rotating 90 degrees");
      for(int posDegrees = 0; posDegrees <= 90; posDegrees++) {
        servo1.write(posDegrees);
        delay(10);
      }
      isWindowOpen = true;
    }
    else {
       Serial.println("Window already open, no action");         
    }
}

static void subscribeCallback (int topicNameLen, char *topicName, int payloadLen, char *payLoad)
{

     //check if the topic is Window/close or Window/open
    rcvdPayload = String(payLoad);
    cmdReceived = CMD_UNKNOWN;
    String topic = String(topicName);
    if ( topic.startsWith(topicClose+ "{")) {
      cmdReceived = CMD_CLOSE;
      rcvdTopic = topicClose;
    }
    else if (topic.startsWith(topicOpen + "{")) {
      cmdReceived = CMD_OPEN;
      rcvdTopic = topicOpen;
    }
    else 
      rcvdTopic = topicName;
    msgReceived = 1;
}


void setup() {
    Serial.begin(115200);
    servo1.attach(servoPin);
    Serial.println("\n\nSubscribe to Window/open and Window/close topic to control the position of a servo motor connect to GPIO 13");
    windowClose();

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

    greengrass = new AWSGreenGrassIoT(AWSIOTURL, THING, aws_root_ca, thingCA, thingKey);

   if(greengrass->connectToGG() == true)
    {
        Serial.println("Connected to AWS GreenGrass");
        delay(2000);

       if( true == greengrass->subscribe(TOPIC_NAME,subscribeCallback)) {
            Serial.println("Subscribe to Window/# topic successful ");
       }
       else {
            Serial.println("Subscribe to Window/# Failed, Check the Thing Name and Certificates");
            while(1);
       }

     }
    else
    {
        Serial.println("Connection to Greengrass failed, check if Greengrass is on and connected to the WiFi");
        while(1);
    }

}



void loop() {
  
    if(msgReceived == 1) {
        msgReceived = 0;
        if (cmdReceived == CMD_OPEN) {
          windowOpen();
        }
        else if (cmdReceived == CMD_CLOSE) {
          windowClose();
        }
        else {
          Serial.println("Received wrong COMMAND");
        }
        Serial.println("Received topic: " + rcvdTopic + rcvdPayload);        
    }
    
}
