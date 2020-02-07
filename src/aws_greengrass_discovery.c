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
 * @file aws_greengrass_discovery.c
 * @brief API for Green grass Discovery. Provides API to Fetch and parse the
 * JSON file that contains the greenGrass code info.
 */


/* Greengrass includes. */
#include "aws_ggd_types.h"
#include "aws_ggd_config.h"
#include "aws_ggd_config_defaults.h"
#include "aws_greengrass_discovery.h"
#include "jsmn.h"

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#define configPRINTF( X )    printf X
/**
 * @brief Discovery: Strings for JSON file parsing.
 *
 * Below are JSON file sections used by the parsing tool to recover
 * the certificate and IP address.
 */
/** @{ */
#define ggdJSON_FILE_GROUPID         "GGGroupId"
#define ggdJSON_FILE_THING_ARN       "thingArn"
#define ggdJSON_FILE_HOST_ADDRESS    "HostAddress"
#define ggdJSON_FILE_CERTIFICATE     "CAs"
#define ggdJSON_FILE_PORT_NUMBER     "PortNumber"
/** @} */

/**
 * @brief HTTP command to retrieve JSON file from the Cloud.
 */
#define ggdCLOUD_DISCOVERY_ADDRESS      \
    "GET /greengrass/discover/thing/%s" \
    " HTTP/1.1\r\n\r\n"

#define ggJSON_CONVERTION_RADIX    10

/**
 * @brief HTTP field to get the length of the JSON file.
 *
 * The server respond with the JSON file encapsulated in
 * a HTTP header. The header is parsed bytes per bytes until
 * "content-length:" is found. then the length string is stored
 * into a temporary buffer of the ggJSON_PARSING_TMP_BUFFER_SIZE
 * size. Then a string to int conversion is performed to get the size
 * of the JSON file.
 */
/** @{ */
#define ggdHTTP_CONTENT_LENGTH_STRING     "content-length:"
#define ggJSON_PARSING_TMP_BUFFER_SIZE    10
/** @} */

/**
 * @brief Size of the IP address character string
 *
 * IP address is 15 character + the null termination.
 */
#define ggdJSON_IP_ADDRESS_SIZE    16

/**
 * @brief Loop back IP
 *
 * Discarded when Parsing JSON file as potential IP to connect to.
 */
#define ggdLOOP_BACK_IP            "127.0.0.1"
#undef _MYDEBUG

/**
 * @brief JSON parsing helper functions.
 *
 * The functions declared below are used to make parsing of JSON file a bit easier.
 * The code is broken down into tiny functions to limit code complexity and limit
 * duplicate code between auto connect and custom connect.
 */
/** @{ */
static BaseType_t prvGGDJsoneq( const char * pcJson,     /*lint !e971 can use char without signed/unsigned. */
                                const jsmntok_t * const pxTok,
                                const char * pcString ); /*lint !e971 can use char without signed/unsigned. */
static void prvCheckMatch( const char * pcJSONFile,      /*lint !e971 can use char without signed/unsigned. */
                           const jsmntok_t * pxTok,
                           const uint32_t ulTokenIndex,
                           BaseType_t * pxMatch,
                           const char * pcMatchCategory,   /*lint !e971 can use char without signuint32_t ulNbTokensed/unsigned. */
                           const char * pcMatchString,     /*lint !e971 can use char without signed/unsigned. */
                           const BaseType_t xAutoSelectFlag );
static BaseType_t prvGGDGetCertificate( char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                        const HostParameters_t * pxHostParameters,
                                        const BaseType_t xAutoSelectFlag,
                                        const jsmntok_t * pxTok,
                                        const uint32_t ulNbTokens,
                                        GGD_HostAddressData_t * pxHostAddressData );
static BaseType_t prvGGDGetIPOnInterface( char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                          const uint8_t ucTargetInterface,
                                          const jsmntok_t * pxTok,
                                          const uint32_t ulNbTokens,
                                          GGD_HostAddressData_t * pxHostAddressData,
                                          uint32_t * pulTokenIndex,
                                          uint8_t * pucCurrentInterface );
static BaseType_t prvGGDGetCore( const char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                 const HostParameters_t * const pxHostParameters,
                                 const BaseType_t xAutoSelectFlag,
                                 const jsmntok_t * pxTok,
                                 const uint32_t ulNbTokens,
                                 uint32_t * pulTokenIndex );
static BaseType_t prvIsIPvalid( const char * pcIP);
/** @} */


/*-----------------------------------------------------------*/

BaseType_t GGD_GetGGCIPandCertificate( const char * pcBuffer, /*lint !e971 can use char without signed/unsigned. */
                                       GGD_HostAddressData_t * pxHostAddressData)
{
    uint32_t ulJSONFileSize = strlen(pcBuffer);
    BaseType_t xStatus;

    configASSERT( pxHostAddressData != NULL );
    configASSERT( pcBuffer != NULL );
 
 //   configPRINTF(("GGD_GetIPAndCertificateFrom JSON...\r\n"));

    xStatus = GGD_GetIPandCertificateFromJSON( pcBuffer,ulJSONFileSize, NULL, pxHostAddressData, pdTRUE );

    return xStatus;
}

/*-----------------------------------------------------------*/

BaseType_t GGD_GetIPandCertificateFromJSON( const char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                            const uint32_t ulJSONFileSize,
                                            const HostParameters_t * pxHostParameters,
                                            GGD_HostAddressData_t * pxHostAddressData,
                                            const BaseType_t xAutoSelectFlag )
{
    BaseType_t xStatus;
    jsmn_parser xParser;
    jsmntok_t pxTok[ ggdconfigJSON_MAX_TOKENS ];
    int32_t lNbTokens;
    uint32_t ulTokenIndex = 0;
    uint8_t ucCurrentInterface = 0, ucTargetInterface = 1;
    BaseType_t xFoundGGC = pdFALSE;
    BaseType_t xIsIPValid;

    configASSERT( pcJSONFile != NULL );
    configASSERT( pxHostAddressData != NULL );

    jsmn_init( &xParser );
    /* From jsmn, parse the JSON file. */
    lNbTokens = ( int32_t ) jsmn_parse( &xParser,
                                        pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                        ( size_t ) ulJSONFileSize,
                                        pxTok,
                                        ( unsigned int ) ggdconfigJSON_MAX_TOKENS ); /*lint !e961 redundant casting only when int = int32_t. */

#ifdef _MYDEBUG
    char tmp[100];
    for (int i = 0; i < lNbTokens; i++ ) {
        int len = pxTok[i].end - pxTok[i].start;
        memset(tmp, 0, 100);
        if ( len < 100) {
            strncpy(tmp, &pcJSONFile[pxTok[i].start], len);
            printf("token %d = %s, type = %d, size = %d, len = %d\n", i, tmp, pxTok[i].type, pxTok[i].size, len);
        }
        else
            printf("big token (%d), type = %d, size = %d, len = %d\n", i, pxTok[i].type, pxTok[i].size, len);
    }
#endif

    /* Manage the case. */
    if( lNbTokens < 0 )
    {
        ggdconfigPRINT( "JSON parsing: Failed to parse JSON\r\n" );

        xStatus = pdFAIL;
    }
    else
    {
        xStatus = pdPASS;
    }

    if( xStatus == pdPASS )
    {
        /* Look for the green grass group certificate. */
        if( prvGGDGetCertificate( (char *) pcJSONFile,
                                  pxHostParameters,
                                  xAutoSelectFlag,
                                  pxTok,
                                  ( uint32_t ) lNbTokens, /*lint !e644 lNbTokens has been initialized if code reaches here. */
                                  pxHostAddressData ) == pdFAIL )
        {
            ggdconfigPRINT( "JSON parsing: Couldn't find certificate\r\n" );

            xStatus = pdFAIL;
        }
    }

    if( xStatus == pdPASS )
    {
        /* Look for the green grass core, green grass core position is returned in ulTokenIndex. */
        if( prvGGDGetCore( (char *) pcJSONFile,
                           pxHostParameters,
                           xAutoSelectFlag,
                           pxTok,
                           ( uint32_t ) lNbTokens,
                           &ulTokenIndex ) == pdFAIL )
        {
            ggdconfigPRINT( "JSON parsing: Couldn't find Green Grass Core\r\n" );

            xStatus = pdFAIL;
        }
    }

    if( xStatus == pdPASS )
    {
        /* If autoSelectFlag is set to true, the code below will try
         * to connect directly to the interface.
         * If autoSelectFlag is set to false, the code below will try
         * to connect to every interfaces. */
        if( xAutoSelectFlag == pdFALSE )
        {
            if( prvGGDGetIPOnInterface( (char *) pcJSONFile,
                                        ucTargetInterface,
                                        pxTok,
                                        ( uint32_t ) lNbTokens,
                                        pxHostAddressData,
                                        &ulTokenIndex,
                                        &ucCurrentInterface ) == pdFAIL )
            {
                ggdconfigPRINT( "GGC - Can't find interface\r\n" );
            }
            else
            {
                xFoundGGC = pdTRUE;
            }
        }
        else
        {
            while( prvGGDGetIPOnInterface( (char *) pcJSONFile,
                                           ucTargetInterface,
                                           pxTok,
                                           ( uint32_t ) lNbTokens,
                                           pxHostAddressData,
                                           &ulTokenIndex,
                                           &ucCurrentInterface ) == pdPASS )
            {
                xIsIPValid = prvIsIPvalid( ( const char * ) pxHostAddressData->pcHostAddress);

                if( xIsIPValid == pdTRUE )
                {
                    ggdconfigPRINT("Discovered Greengrass IP address %s, port: %d\n", pxHostAddressData->pcHostAddress, pxHostAddressData->usPort);
                    xFoundGGC = pdTRUE;
                    break;
                }

                ucTargetInterface++;
            }
        }

        if( xFoundGGC != pdTRUE )
        {
            ggdconfigPRINT( "GGD - Can't find greengrass Core\r\n" );

            xStatus = pdFAIL;
        }
    }

    return xStatus;
}
/*-----------------------------------------------------------*/

/* Return true if the string " pcString" is found inside the token pxTok in JSON file pcJson. */
static BaseType_t prvGGDJsoneq( const char * pcJson,    /*lint !e971 can use char without signed/unsigned. */
                                const jsmntok_t * const pxTok,
                                const char * pcString ) /*lint !e971 can use char without signed/unsigned. */
{
    uint32_t ulStringSize = ( uint32_t ) pxTok->end - ( uint32_t ) pxTok->start;
    BaseType_t xStatus = pdFALSE;


    if( pxTok->type == JSMN_STRING )
    {

        if( ( uint32_t ) strlen( pcString ) == ulStringSize )
        {
            if( ( int16_t ) strncmp( &pcJson[ pxTok->start ],
                                     pcString,
                                     ulStringSize ) == 0 )
            {
                xStatus = pdTRUE;
            }
        }

    }

    return xStatus;
}
/*-----------------------------------------------------------*/


static void prvCheckMatch( const char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                           const jsmntok_t * pxTok,
                           const uint32_t ulTokenIndex,
                           BaseType_t * pxMatch,
                           const char * pcMatchCategory, /*lint !e971 can use char without signed/unsigned. */
                           const char * pcMatchString,   /*lint !e971 can use char without signed/unsigned. */
                           const BaseType_t xAutoSelectFlag )
{
    *pxMatch = prvGGDJsoneq( pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                             &pxTok[ ulTokenIndex ],
                             pcMatchCategory );

    if( *pxMatch == pdTRUE )
    {
        /* Is that the group we are looking for? */
        if( xAutoSelectFlag != pdTRUE )
        {
            if( ( prvGGDJsoneq( pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                &pxTok[ ulTokenIndex + ( uint32_t ) 1 ],
                                pcMatchString ) == pdTRUE ) )
            {
                *pxMatch = pdTRUE;
            }
            else
            {
                *pxMatch = pdFALSE;
            }
        }
        else
        {
            *pxMatch = pdTRUE;
        }
    }
}
/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetCore( const char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                 const HostParameters_t * const pxHostParameters,
                                 const BaseType_t xAutoSelectFlag,
                                 const jsmntok_t * pxTok,
                                 const uint32_t ulNbTokens,
                                 uint32_t * pulTokenIndex )
{
    BaseType_t xStatus = pdFAIL;
    BaseType_t xMatchGroup = pdFALSE;
    BaseType_t xMatchCore = pdFALSE;

    /* Look for the green grass core inside the group. */
    for( *pulTokenIndex = 0; *pulTokenIndex < ulNbTokens; ( *pulTokenIndex )++ )
    {
        /* Check if group matches. */
        if( xAutoSelectFlag == pdTRUE )
        {
            xMatchGroup = pdTRUE;
            xMatchCore = pdTRUE;
        }
        else
        {
            if( xMatchGroup != pdTRUE )
            {
                prvCheckMatch( pcJSONFile,
                               pxTok,
                               *pulTokenIndex,
                               &xMatchGroup,
                               ggdJSON_FILE_GROUPID,
                               pxHostParameters->pcGroupName, /*lint !e971 can use char without signed/unsigned. */
                               xAutoSelectFlag );
            }
            else
            {
                /* Check if core matches. */
                prvCheckMatch( pcJSONFile,
                               pxTok,
                               *pulTokenIndex,
                               &xMatchCore,
                               ggdJSON_FILE_THING_ARN,
                               pxHostParameters->pcCoreAddress, /*lint !e971 can use char without signed/unsigned. */
                               xAutoSelectFlag );
            }
        }

        /* We have a match! correct core and correct group! */
        if( ( xMatchCore == pdTRUE ) && ( xMatchGroup == pdTRUE ) )
        {
            /* We have found the Core, now break and try to connect to the interface. */
            xStatus = pdPASS;
            break;
        }
    }

    /* Green grass core not found. */
    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetCertificate( char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                        const HostParameters_t * pxHostParameters,
                                        const BaseType_t xAutoSelectFlag,
                                        const jsmntok_t * pxTok,
                                        const uint32_t ulNbTokens,
                                        GGD_HostAddressData_t * pxHostAddressData )
{
    BaseType_t xMatchGroup = pdFALSE;
    uint32_t ulTokenIndex;
    uint32_t ulReadIndex = 0, ulWriteIndex = 0;
    BaseType_t xStatus = pdFAIL;

    /* Loop over all keys of the root object. */
    for( ulTokenIndex = 0; ulTokenIndex < ulNbTokens; ulTokenIndex++ )
    {
        /* Check if group matches. */
        if( xAutoSelectFlag == pdTRUE )
        {
            xMatchGroup = pdTRUE;
        }

        if( xMatchGroup != pdTRUE )
        {
            prvCheckMatch( (const char *) pcJSONFile, pxTok,
                           ulTokenIndex,
                           &xMatchGroup,
                           ggdJSON_FILE_GROUPID,
                           pxHostParameters->pcGroupName,
                           xAutoSelectFlag );
        }
        else
        {
            if( prvGGDJsoneq( (const char *) pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                              &pxTok[ ulTokenIndex ],
                              ggdJSON_FILE_CERTIFICATE ) == pdTRUE )
            {
                pxHostAddressData->pcCertificate =
                    &pcJSONFile[ pxTok[ ulTokenIndex + ( uint32_t ) 1 ].start ];
                /* Skip 2 brackets at the beginning that are not used in certificate. */
                pxHostAddressData->pcCertificate =
                    &pxHostAddressData->pcCertificate[ 2 ];

                /* Remove 2 that correspond to the skipped brackets. */
                pxHostAddressData->ulCertificateSize = ( ( uint32_t ) pxTok[ ulTokenIndex + ( uint32_t ) 1 ].end
                                                         - ( uint32_t ) pxTok[ ulTokenIndex + ( uint32_t ) 1 ].start )
                                                       - ( uint32_t ) 2;
                ulWriteIndex = 0;

                /* This section will convert the certificate from the JSON file into a certificate that can be
                 * given to the TLS service. For that all the \\ has to be replaced by \n. */
                ulReadIndex = 1;

                do
                {
                    if( ( pxHostAddressData->pcCertificate[ ulReadIndex - ( uint32_t ) 1 ] == '\\' ) &&
                        ( pxHostAddressData->pcCertificate[ ulReadIndex ] == 'n' ) )
                    {
                        pxHostAddressData->pcCertificate[ ulWriteIndex ] = '\n';
                        ulReadIndex++;
                    }
                    else
                    {
                        pxHostAddressData->pcCertificate[ ulWriteIndex ] =
                            pxHostAddressData->pcCertificate[ ulReadIndex - ( uint32_t ) 1 ];
                    }

                    ulReadIndex++;
                    ulWriteIndex++;
                }
                while( ulReadIndex < pxHostAddressData->ulCertificateSize );

                pxHostAddressData->ulCertificateSize = ulWriteIndex;
                pxHostAddressData->pcCertificate[ ulWriteIndex - ( uint32_t ) 1 ] = '\0';

                xStatus = pdPASS;
            }
        }
    }

    return xStatus;
}
/*-----------------------------------------------------------*/
/*
    Return true if the IP address is not a local 127.0.0.0 and if is not an IPv6 address
    (in this case checks the existence of '::' sequence)
 */
static BaseType_t prvIsIPvalid( const char * pcIP)
{
    BaseType_t xStatus;

    if( strcmp( ggdLOOP_BACK_IP, pcIP ) != 0 ) {
        xStatus = pdTRUE;
        if ( !strstr(pcIP, "::"))
            xStatus = pdTRUE;
        else
            xStatus = pdFALSE;

    } else {
        xStatus = pdFALSE;
    }

    return xStatus;
}

/*-----------------------------------------------------------*/

static BaseType_t prvGGDGetIPOnInterface( char * pcJSONFile, /*lint !e971 can use char without signed/unsigned. */
                                          const uint8_t ucTargetInterface,
                                          const jsmntok_t * pxTok,
                                          const uint32_t ulNbTokens,
                                          GGD_HostAddressData_t * pxHostAddressData,
                                          uint32_t * pulTokenIndex,
                                          uint8_t * pucCurrentInterface )
{
    BaseType_t xStatus = pdFAIL;
    BaseType_t xFoundIP = pdFALSE;
    BaseType_t xFoundPort = pdFALSE;

    /* From the green grass core position. */
    for( ; *pulTokenIndex < ulNbTokens; ( *pulTokenIndex )++ )
    {
        if( prvGGDJsoneq( pcJSONFile, &pxTok[ *pulTokenIndex ], /*lint !e971 can use char without signed/unsigned. */
                          ggdJSON_FILE_HOST_ADDRESS ) == pdTRUE )
        {
            xFoundIP = pdTRUE;
            pxHostAddressData->pcHostAddress =
                &pcJSONFile[ pxTok[ *pulTokenIndex + ( uint32_t ) 1 ].start ]; /*lint !e971 can use char without signed/unsigned. */
            pcJSONFile[ pxTok[ *pulTokenIndex + ( uint32_t ) 1 ].end ] = '\0'; /* End with a null  character. */
        }

        if( prvGGDJsoneq( pcJSONFile, &pxTok[ *pulTokenIndex ], /*lint !e971 can use char without signed/unsigned. */
                          ggdJSON_FILE_PORT_NUMBER ) == pdTRUE )
        {
            pxHostAddressData->usPort = ( uint16_t ) strtoul( &pcJSONFile[ pxTok[ *pulTokenIndex + ( uint32_t ) 1 ].start ], NULL, ggJSON_CONVERTION_RADIX );
            xFoundPort = pdTRUE;
        }

        /* Get host IP address. */
        if( ( xFoundIP == pdTRUE ) && ( xFoundPort == pdTRUE ) )
        {
            xFoundIP = pdFALSE;
            xFoundPort = pdFALSE;
            ( *pucCurrentInterface )++;

            if( *pucCurrentInterface == ucTargetInterface )
            {
                xStatus = pdPASS;
                break;
            }
        }
    }

    ( *pulTokenIndex )++; /* Increase index to avoid avoid a match next time the function is called */
    return xStatus;
}
/*-----------------------------------------------------------*/
