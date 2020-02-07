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

#ifndef _AWS_GGD_TYPES_
#define _AWS_GGD_TYPES_

typedef unsigned int                                        uint32_t;
typedef signed int                                          int32_t;
typedef long												BaseType_t;
typedef unsigned long										UBaseType_t;
typedef unsigned char                                       uint8_t;
typedef unsigned short int                                  uint16_t;
typedef short signed int                                    int16_t;

#define pdFALSE			( ( BaseType_t ) 0 )
#define pdTRUE			( ( BaseType_t ) 1 )

#define pdPASS			( pdTRUE )
#define pdFAIL			( pdFALSE )

#ifndef configASSERT
	#define configASSERT( x )
	#define configASSERT_DEFINED 0
#else
	#define configASSERT_DEFINED 1
#endif

#endif
