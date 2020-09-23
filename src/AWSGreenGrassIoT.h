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

#ifndef _AWSGREENGRASS_H_
#define _AWSGREENGRASS_H_

#include "aws_greengrass_discovery.h"
#include "aws_iot_mqtt_client.h"

typedef void (*pSubCallBackHandler_t)(int topicNameLen, char *topicName, int payloadLen, char *payLoad);

class AWSGreenGrassIoT  {

public:
  AWSGreenGrassIoT( const char * AwsIoTCoreurl, // AWS IoT core URL
                    const char * thingName,     // AWS thing name
                    const char * iotCoreCA,     // AWS IoT core certificate (defined in certificate.c)
                    const char * thingCA,       // thing certificate (defined in certificates.c)
                    const char * thingKey);     // thing private key (defined in certificate.c)

  ~AWSGreenGrassIoT();

  bool connectToGG(void);
  bool connectToIoTCore(void);

  bool publish(char *pubtopic, char *pubPayLoad);
  bool publishBinary( char * pubtopic, char * payload, int payloadLength);
  bool subscribe(char *subTopic, pSubCallBackHandler_t pSubCallBackHandler);

  bool isConnected() { return _connected;}

  static void taskRunner(void *);

  void disconnect() { _connected = false; _isGGDiscovered=false;}

protected:
  int _connect( char * host,  char * rootCA);
  bool discoverGG(void);

private:

  char * _iotCoreUrl;
  char * _thingName;
  GGD_HostAddressData_t _HostAddressData;
  bool _isGGDiscovered;
  bool _connected;

  char * _iotCoreCA;
  char * _thingCA;
  char * _thingKey;
  int _port = 8883;

};

#endif
