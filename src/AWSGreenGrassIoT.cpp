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

#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "aws_greengrass_discovery.h"

#include "AWSGreenGrassIoT.h"

pSubCallBackHandler_t subApplCallBackHandler = 0;
AWS_IoT_Client _client;   /* mqttClient structure cannot be a member function otherwise it will generate compiler errors in the taskRunner */

static void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;
    AWSGreenGrassIoT * pGreengrass = (AWSGreenGrassIoT  *) data;

    Serial.println("Disconnect callback handler");

	if(NULL == pClient) {
        /* if the IoT Client is null, it is not possible to reconnect automatically
        so we set the internal flag of the greengrass object to false */ 
        pGreengrass->disconnect();
 	    IOT_WARN("Reconnect cannot take place because client is NULL");
		return;
	}


	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
            /* in case the reconnection doesn't work, set the flag of the greengrass object accordingly */
            pGreengrass->disconnect();
          
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}


static void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("Subscribe callback");
	IOT_INFO("%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *) params->payload);
    if (subApplCallBackHandler)
        subApplCallBackHandler(topicNameLen, topicName, params->payloadLen,(char *)params->payload);
}

AWSGreenGrassIoT::AWSGreenGrassIoT(const char * AwsIoTCoreurl, // AWS IoT core URL
                    const char * thingName,     // AWS thing name
                    const char * iotCoreCA,     // AWS IoT core certificate (defined in certificate.c)
                    const char * thingCA,       // thing certificate (defined in certificates.c)
                    const char * thingKey)      // thing private key (defined in certificate.c)
{

    _iotCoreUrl = (char *) pvPortMalloc(strlen(AwsIoTCoreurl)+1);
    strcpy(_iotCoreUrl, AwsIoTCoreurl);
    _thingName = (char *) pvPortMalloc(strlen(thingName)+1);;
    strcpy(_thingName, thingName);

    _connected = false;
    _isGGDiscovered = false;
    _iotCoreCA = (char *) iotCoreCA;
    _thingCA = (char *) thingCA;
    _thingKey = (char *) thingKey;
}

/*
    the connect member function can be used either to connect to Greengrass or
    directly to AWS IoT core (depending on the host address and the root Certificate)
*/

int AWSGreenGrassIoT::_connect( char * host,  char * rootCA) {

	IoT_Error_t rc = FAILURE;
    _connected = false;
    const size_t stack_size = 36*1024;

	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", _thingCA);
	IOT_DEBUG("clientKey %s", _thingKey);
	mqttInitParams.enableAutoReconnect = true;
	mqttInitParams.pHostURL = host;
	mqttInitParams.port = _port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = _thingCA;
	mqttInitParams.pDevicePrivateKeyLocation = _thingKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;

    if (strcmp(host, _iotCoreUrl) == 0)
	    mqttInitParams.isSSLHostnameVerify = true;
    else
 	    mqttInitParams.isSSLHostnameVerify = false;  // in case of Greengrass, the host certificate doesn't have to be verified

	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = this;

	rc = aws_iot_mqtt_init(&_client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 600;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
    if (connectParams.clientIDLen = (uint16_t) strlen(_thingName))
	connectParams.pClientID = _thingName;
	else
        {
        connectParams.clientIDLen = 0;
        connectParams.pClientID = NULL;
        }
    
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&_client, &connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
	}
    else
        _connected = true;

     if(rc == SUCCESS)
        xTaskCreate(&taskRunner, "AWSGreenGrassIoTTask", stack_size, NULL, 6, NULL);

	return rc;
}


bool AWSGreenGrassIoT::publish(char *pubtopic, char *pubPayLoad) {

    IoT_Publish_Message_Params paramsQOS;
    paramsQOS.qos = QOS0;
    paramsQOS.payload = (void *) pubPayLoad;
    paramsQOS.isRetained = 0;
    paramsQOS.payloadLen = strlen(pubPayLoad);
	IoT_Error_t rc = FAILURE;
    bool ret = false;

    if (_connected) {

        rc = aws_iot_mqtt_publish(&_client, pubtopic, strlen(pubtopic), &paramsQOS);
        if (rc != SUCCESS) {
            IOT_WARN("Publish ack not received.\n");
        }
        else
            ret = true;

    }
    return ret;
 }

bool AWSGreenGrassIoT::publishBinary(char *pubtopic, char *pubPayLoad, int payloadLength) {

    IoT_Publish_Message_Params paramsQOS;
    paramsQOS.qos = QOS0;
    paramsQOS.payload = (void *) pubPayLoad;
    paramsQOS.isRetained = 0;
    paramsQOS.payloadLen = payloadLength;
	IoT_Error_t rc = FAILURE;
    bool ret = false;

    if (_connected) {

        rc = aws_iot_mqtt_publish(&_client, pubtopic, payloadLength, &paramsQOS);
        if (rc != SUCCESS) {
            IOT_WARN("Publish ack not received.\n");
        }
        else
            ret = true;

    }
    return ret;
 }

bool AWSGreenGrassIoT::subscribe(char *subTopic, pSubCallBackHandler_t pSubCallBackHandler) {
	IoT_Error_t rc = FAILURE;
    bool ret = false;
    subApplCallBackHandler = pSubCallBackHandler;

    if (_connected) {
        ESP_LOGI(TAG, "Subscribing...");
        rc = aws_iot_mqtt_subscribe(&_client, subTopic, strlen(subTopic), QOS0, iot_subscribe_callback_handler, NULL);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error subscribing : %d ", rc);
            return false;
        }
        ESP_LOGI(TAG, "Subscribing... Successful");
        ret = true;
    }
    return ret;
}

AWSGreenGrassIoT::~AWSGreenGrassIoT(void)
{
    vPortFree(_iotCoreUrl);
    vPortFree(_thingName);
}


bool AWSGreenGrassIoT::discoverGG(void) {

    String greenGrassDiscoveryUrl;
    HTTPClient https;
    WiFiClientSecure * _wfclient;

    _wfclient = new WiFiClientSecure;
    _wfclient->setCACert(_iotCoreCA);
    _wfclient->setCertificate(_thingCA);
    _wfclient->setPrivateKey(_thingKey);

/*
      Generate  green grass discovery url:
             https://XXXXXXXXXXXXX-ats.iot.region.amazonaws.com:port/greengrass/discover/thing/thing-name
*/

    greenGrassDiscoveryUrl = String("https://") + String(_iotCoreUrl) + String(":8443/greengrass/discover/thing/") + String(_thingName);

    _isGGDiscovered = false;
    if (https.begin(*_wfclient, greenGrassDiscoveryUrl))
    {

        // start connection and send HTTP header
        int httpCode = https.GET();

        // httpCode will be negative on error
        if (httpCode > 0)
        {
            // HTTP header has been send and Server response header has been handled
            ESP_LOGI(TAG,"[HTTPS] GET... successful\n");

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                String payload = https.getString();
                Serial.printf("Response from greengrass discovery\n");

                // extract the Greengrass host address and greengrass root certificate
                _isGGDiscovered = GGD_GetGGCIPandCertificate(payload.c_str(), &_HostAddressData);
            }
        }
        else
        {
            Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }

        https.end();
    }
    else
    {
        Serial.printf("[HTTPS] Unable to connect\n");
    }

    delete _wfclient;
    return _isGGDiscovered;
}

bool AWSGreenGrassIoT::connectToGG(void) {

    if (!_connected)
    {
        if (!_isGGDiscovered)
            {
                if (discoverGG())
                {
                    if (_connect( (char *) _HostAddressData.pcHostAddress, (char *) _HostAddressData.pcCertificate ) == 0)
                        _connected = true;
                    else {
                        Serial.println("Failed to connect to Greengrass");
                    }

                }
                else
                {
                    Serial.println("Greengrass Discovery failed");
                }

            }
    }
    return _connected;
}

bool AWSGreenGrassIoT::connectToIoTCore(void) {

    if (!_connected)
    {
        if (_connect(_iotCoreUrl, _iotCoreCA) == 0)
            _connected = true;
        else
        {
            Serial.println("Failed to connect to AWS IoT Core");
        }
    }

    return _connected;
}

void AWSGreenGrassIoT::taskRunner( void * param) {
    IoT_Error_t rc = SUCCESS;
    while(1)
    {
        //allocate some time to read messages from IoT broker
        rc = aws_iot_mqtt_yield( &_client, 400);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            continue;
        }

        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
