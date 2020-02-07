/*
 * Amazon FreeRTOS Greengrass V1.0.5
 * Copyright (C) 2018 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file aws_greengrass_discovery.h
 * @brief greengrass discovery API.
 */
#ifdef __cplusplus
extern "C" {
#endif


#ifndef _AWS_GREENGRASS_DISCOVERY_H_
#define _AWS_GREENGRASS_DISCOVERY_H_

/**
 * @brief Input from user to locate GGC inside JSON file.
 *
 * When using manual selection discovery, the user should fill the host
 * parameters fields. The discovery API will use those to
 * locate the GGC inside the JSON file and fill
 * GGD_HostAddressData_t fields.
 */
typedef struct
{
    const char * pcGroupName;   /**< Contains the Group name of the GGC device. */
    const char * pcCoreAddress; /**< Contains the ARN of the GGC device. */
    uint8_t ucInterface;        /**< Contains the interface # of the GGC device. */
} HostParameters_t;

/**
 * @brief Green Grass Core connection parameters.
 *
 * Green Grass Core connection parameters extracted from the JSON file
 * returned from the local discovery or from the Cloud.
 * This is the return parameter from the discovery.
 */
typedef struct
{
    const char * pcHostAddress; /**< Host address, could be IP or hostname. */
    char * pcCertificate;       /**< Certificate of GGC. */
    uint32_t ulCertificateSize; /**< Certificate size of GGC. */
    uint16_t usPort;            /**< Port to connect to the GGC. */
} GGD_HostAddressData_t;

/*
 * @brief Connect directly to the green grass core.
 *
 * @note: In most case only calling this function is needed!
 * This function will perform in series:
 * 1. GGD_JSONRequest.
 * 2. GGD_GetJSONFileSize.
 * 3. GGD_GetJSONFile.
 * 4. GGD_ConnectToHost with auto selection parameters set to true.
 * The buffer size of pcBuffer need to be big enough to hold the complete
 * JSON file.
 *
 * @param [in] pcBuffer: Memory buffer provided by the user.
 *
 * @param [in] ulBufferSize: Size of the memory buffer.
 *
 * @param [out] pxHostAddressData : host address data
 *
 * @return If connection was successful then pdPASS is
 * returned.  Otherwise pdFAIL is returned.
 */
BaseType_t GGD_GetGGCIPandCertificate(  const char * pcBuffer,
                                       GGD_HostAddressData_t * pxHostAddressData);

/*
 * @brief  Get host IP and certificate
 *
 * Get host IP and certificate with the JSON file given the greengrass group
 * and cloud core address the user wants to connect.
 *
 * @note:  The JSON file contains the certificate that is going to be used to
 * connect with the core. For memory savings, the JSON file certificate is
 * going to be editing inline to connect with the host.
 * Thus, afterwards, the JSON will become unusable.
 * This function will also change the default certificate being used.
 * The default certificate is the one specified in client credential.
 * After calling this function, it will be one of the certificates provided in the
 * JSON file.
 * @note that if xAutoSelectFlag is set to false, the user will have to provide the
 * pxHostParameters. If xAutoSelectFlag is set to true, then pxHostParameters
 * are not used (can be set to NULL) and the IP and certificate returned will be of
 * the first established connection.
 * The user would also need to specify the ggdconfigCORE_NETWORK_INTERFACE (starting from 1)
 * in aws_ggd_config.h
 *
 * @param [out] pxHostAddressData : host address data
 *
 * @param [in] pucJSON_File: Pointer to the JSON file. WARNING, will be modified to
 *             format the certificate.
 *
 * @param [in] ulJSON_File_Size: Size in byte of the array.
 *
 * @param [in] pxHostParameters: Contains the group name, cloud address of the desired
 * core to connect to and interface to use (WIFI, ETH0 etc...).
 * @warning: Cannot be NULL if xAutoSelectFlag is set to pdFALSE
 *
 * @param [in] xAutoSelectFlag: The user can opt for the auto select option.
 *             Then pxHostParameters are not used. Can be set to NULL.
 *
 * @return successfull pdPASS is
 * returned.  Otherwise pdFAIL is returned.
 */
BaseType_t GGD_GetIPandCertificateFromJSON( const char * pcJSONFile,
                                            const uint32_t ulJSONFileSize,
                                            const HostParameters_t * pxHostParameters,
                                            GGD_HostAddressData_t * pxHostAddressData,
                                            const BaseType_t xAutoSelectFlag );
#endif /* _AWS_GREENGRASS_DISCOVERY_H_ */

#ifdef __cplusplus
} /* extern "C" */
#endif
