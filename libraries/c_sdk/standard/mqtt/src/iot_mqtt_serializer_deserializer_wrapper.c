/*
 * FreeRTOS MQTT V2.1.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * @file iot_mqtt_serializer_deserializer_wrapper.c
 * @brief Implements serializer and deserializer wrapper functions for the shim.
 */
/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <string.h>

/* Error handling include. */
#include "private/iot_error.h"

/* MQTT internal includes. */
#include "private/iot_mqtt_internal.h"

/* MQTT v4_beta 2 lightweight library includes. */
#include "mqtt_lightweight.h"

/* Atomic operations. */
#include "iot_atomic.h"

/*-----------------------------------------------------------*/

/*Size of Puback packet */
#define MQTT_PACKET_PUBACK_SIZE    ( 4 )

/*-----------------------------------------------------------*/

/*Generate Id for packet*/
static uint16_t _nextPacketIdentifier( void );

/*-----------------------------------------------------------*/

/*Generate Id for packet*/
static uint16_t _nextPacketIdentifier( void )
{
    /* MQTT specifies 2 bytes for the packet identifier; however, operating on
     * 32-bit integers is generally faster. */
    static uint32_t nextPacketIdentifier = 1;

    /* The next packet identifier will be greater by 2. This prevents packet
     * identifiers from ever being 0, which is not allowed by MQTT 3.1.1. Packet
     * identifiers will follow the sequence 1,3,5...65535,1,3,5... */
    return ( uint16_t ) Atomic_Add_u32( &nextPacketIdentifier, 2 );
}

/*-----------------------------------------------------------*/

/* Connect Serialize Wrapper*/
IotMqttError_t _connectSerializeWrapper( const IotMqttConnectInfo_t * pConnectInfo,
                                         uint8_t ** pConnectPacket,
                                         size_t * pPacketSize )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    size_t remainingLength = 0UL;
    MQTTConnectInfo_t connectInfo;
    MQTTFixedBuffer_t networkBuffer;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPublishInfo_t willInfo;

    /* Null Check for connectInfo  */
    IotMqtt_Assert( pConnectInfo != NULL );

    connectInfo.cleanSession = pConnectInfo->cleanSession;
    connectInfo.keepAliveSeconds = pConnectInfo->keepAliveSeconds;
    connectInfo.pClientIdentifier = pConnectInfo->pClientIdentifier;
    connectInfo.clientIdentifierLength = pConnectInfo->clientIdentifierLength;
    connectInfo.pUserName = pConnectInfo->pUserName;
    connectInfo.userNameLength = pConnectInfo->userNameLength;
    connectInfo.pPassword = pConnectInfo->pPassword;
    connectInfo.passwordLength = pConnectInfo->passwordLength;
    const MQTTPublishInfo_t * pWillInfo = pConnectInfo->pWillInfo != NULL ? &willInfo : NULL;

    /*NULL Check willInfo */
    if( pWillInfo != NULL )
    {
        willInfo.retain = pConnectInfo->pWillInfo->retain;
        willInfo.pTopicName = pConnectInfo->pWillInfo->pTopicName;
        willInfo.topicNameLength = pConnectInfo->pWillInfo->topicNameLength;
        willInfo.pPayload = pConnectInfo->pWillInfo->pPayload;
        willInfo.payloadLength = pConnectInfo->pWillInfo->payloadLength;
        willInfo.qos = ( MQTTQoS_t ) pConnectInfo->pWillInfo->qos;
    }

    /* Getting Connect packet size using MQTT V4_beta2 API*/

    mqttStatus = MQTT_GetConnectPacketSize( &connectInfo,
                                            pWillInfo,
                                            &remainingLength,
                                            pPacketSize );
    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        /*Allocating memory for Connect packet*/
        networkBuffer.pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.size = *pPacketSize;

        /*Serializing the connect packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializeConnect( &connectInfo,
                                            pWillInfo,
                                            remainingLength,
                                            &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        status = convertReturnCode( mqttStatus );
    }

    if( status == IOT_MQTT_SUCCESS )
    {
        *pConnectPacket = networkBuffer.pBuffer;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Disconnect Serialize Wrapper*/
IotMqttError_t _disconnectSerializeWrapper( uint8_t ** pDisconnectPacket,
                                            size_t * pPacketSize )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTFixedBuffer_t networkBuffer;

    MQTTStatus_t mqttStatus = MQTTSuccess;

    /* Getting Disconnect packet size using MQTT V4_beta2 API*/
    mqttStatus = MQTT_GetDisconnectPacketSize( pPacketSize );

    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        /* Allocate memory to hold the Disconnect packet. */
        networkBuffer.pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.size = *pPacketSize;

        /*Serializing the Disconnect packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializeDisconnect( &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        status = convertReturnCode( mqttStatus );
    }

    if( status == IOT_MQTT_SUCCESS )
    {
        *pDisconnectPacket = networkBuffer.pBuffer;
    }

    return status;
}
/*-----------------------------------------------------------*/

/* Subscribe Serialize Wrapper*/
IotMqttError_t _subscribeSerializeWrapper( const IotMqttSubscription_t * pSubscriptionList,
                                           size_t subscriptionCount,
                                           uint8_t ** pSubscribePacket,
                                           size_t * pPacketSize,
                                           uint16_t * pPacketIdentifier )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    size_t remainingLength = 0UL;
    MQTTSubscribeInfo_t * subscriptionList = IotMqtt_MallocMessage( sizeof( MQTTSubscribeInfo_t ) * subscriptionCount );
    size_t i = 0;
    MQTTFixedBuffer_t networkBuffer;
    uint16_t packetId = 0;

    for( i = 0; i < subscriptionCount; i++ )
    {
        subscriptionList[ i ].qos = ( MQTTQoS_t ) ( pSubscriptionList + i )->qos;
        subscriptionList[ i ].pTopicFilter = ( pSubscriptionList + i )->pTopicFilter;
        subscriptionList[ i ].topicFilterLength = ( pSubscriptionList + i )->topicFilterLength;
    }

    /* Getting Subscribe packet size  using MQTT V4_beta2 API*/
    mqttStatus = MQTT_GetSubscribePacketSize( subscriptionList,
                                              subscriptionCount,
                                              &remainingLength,
                                              pPacketSize );

    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        /*Generating the packet id for subscribe packet*/
        packetId = _nextPacketIdentifier();


        /*Allocating memory for subscribe packet*/
        networkBuffer.pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.size = *pPacketSize;

        /*Serializing the Subscribe packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializeSubscribe( subscriptionList,
                                              subscriptionCount,
                                              packetId,
                                              remainingLength,
                                              &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        status = convertReturnCode( mqttStatus );
    }

    if( status == IOT_MQTT_SUCCESS )
    {
        *pSubscribePacket = networkBuffer.pBuffer;
        *pPacketIdentifier = packetId;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Unsubscribe Serialize Wrapper*/
IotMqttError_t _unsubscribeSerializeWrapper( const IotMqttSubscription_t * pSubscriptionList,
                                             size_t subscriptionCount,
                                             uint8_t ** pUnsubscribePacket,
                                             size_t * pPacketSize,
                                             uint16_t * pPacketIdentifier )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;

    size_t remainingLength = 0UL;
    MQTTSubscribeInfo_t * subscriptionList = IotMqtt_MallocMessage( sizeof( MQTTSubscribeInfo_t ) * subscriptionCount );
    size_t i = 0;
    uint16_t packetId = 0;
    MQTTFixedBuffer_t networkBuffer;

    for( i = 0; i < subscriptionCount; i++ )
    {
        subscriptionList[ i ].qos = ( MQTTQoS_t ) ( pSubscriptionList + i )->qos;
        subscriptionList[ i ].pTopicFilter = ( pSubscriptionList + i )->pTopicFilter;
        subscriptionList[ i ].topicFilterLength = ( pSubscriptionList + i )->topicFilterLength;
    }

    /* Getting Unsubscribe packet size  using MQTT V4_beta2 API*/
    mqttStatus = MQTT_GetUnsubscribePacketSize( subscriptionList,
                                                subscriptionCount,
                                                &remainingLength,
                                                pPacketSize );

    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        /*Generating the packet id for subscribe packet*/
        packetId = _nextPacketIdentifier();


        /*Allocating memory for unsubscribe packet*/
        networkBuffer.pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.size = *pPacketSize;

        /*Serializing the Unsubscribe packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializeUnsubscribe( subscriptionList,
                                                subscriptionCount,
                                                packetId,
                                                remainingLength,
                                                &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        status = convertReturnCode( mqttStatus );
    }

    if( status == IOT_MQTT_SUCCESS )
    {
        *pUnsubscribePacket = networkBuffer.pBuffer;
        *pPacketIdentifier = packetId;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Publish Serialize Wrapper*/
IotMqttError_t _publishSerializeWrapper( const IotMqttPublishInfo_t * pPublishInfo,
                                         uint8_t ** pPublishPacket,
                                         size_t * pPacketSize,
                                         uint16_t * pPacketIdentifier,
                                         uint8_t ** pPacketIdentifierHigh )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    size_t remainingLength = 0UL;
    uint8_t * pBuffer = NULL;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPublishInfo_t publishInfo;
    uint16_t packetId = 0;
    MQTTFixedBuffer_t networkBuffer;

    /* Null Check for publishInfo  */
    IotMqtt_Assert( pPublishInfo != NULL );

    publishInfo.retain = pPublishInfo->retain;
    publishInfo.pTopicName = pPublishInfo->pTopicName;
    publishInfo.topicNameLength = pPublishInfo->topicNameLength;
    publishInfo.pPayload = pPublishInfo->pPayload;
    publishInfo.payloadLength = pPublishInfo->payloadLength;
    publishInfo.qos = ( MQTTQoS_t ) pPublishInfo->qos;

    if( pPublishInfo->qos == IOT_MQTT_QOS_1 )
    {
        publishInfo.dup = true;
    }
    else
    {
        publishInfo.dup = false;
    }

    /* Getting publish packet size  using MQTT V4_beta2 API*/
    mqttStatus = MQTT_GetPublishPacketSize( &publishInfo,
                                            &remainingLength,
                                            pPacketSize );

    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        /*Generating the packet id for publish packet*/
        packetId = _nextPacketIdentifier();


        /*Allocating memory to hold publish packet*/
        pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.pBuffer = pBuffer;
        networkBuffer.size = *pPacketSize;

        /*Serializing the publish packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializePublish( &publishInfo,
                                            packetId,
                                            remainingLength,
                                            &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        status = convertReturnCode( mqttStatus );
    }

    if( status == IOT_MQTT_SUCCESS )
    {
        *pPublishPacket = networkBuffer.pBuffer;
        *pPacketIdentifier = packetId;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Pingreq Serialize Wrapper*/
IotMqttError_t _pingreqSerializeWrapper( uint8_t ** pPingreqPacket,
                                         size_t * pPacketSize )
{
    IotMqttError_t serializeStatus = IOT_MQTT_SUCCESS;

    MQTTFixedBuffer_t networkBuffer;

    MQTTStatus_t mqttStatus = MQTTSuccess;

    /* Getting pingrequest packet size  using MQTT V4_beta2 API*/
    mqttStatus = MQTT_GetPingreqPacketSize( pPacketSize );

    /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
    serializeStatus = convertReturnCode( mqttStatus );

    if( serializeStatus == IOT_MQTT_SUCCESS )
    {
        /* Allocate memory to hold the Pingrequest packet. */
        networkBuffer.pBuffer = IotMqtt_MallocMessage( *pPacketSize );
        networkBuffer.size = *pPacketSize;

        /*Serializing the pingrequest packet using MQTT V4_beta2 API*/
        mqttStatus = MQTT_SerializePingreq( &( networkBuffer ) );

        /*Converting status code from MQTT v4_beta2 status to MQTT v4_beta 1 status*/
        serializeStatus = convertReturnCode( mqttStatus );
    }

    if( serializeStatus == IOT_MQTT_SUCCESS )
    {
        *pPingreqPacket = networkBuffer.pBuffer;
    }

    return serializeStatus;
}

/*-----------------------------------------------------------*/

/*Deserialize Connack Wrapper .*/
IotMqttError_t _deserializeConnackWrapper( _mqttPacket_t * pConnack )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;
    bool sessionPresent = false;

    /* Null Check for connack packet  */
    IotMqtt_Assert( pConnack != NULL );

    pIncomingPacket.type = pConnack->type;
    pIncomingPacket.pRemainingData = pConnack->pRemainingData;
    pIncomingPacket.remainingLength = pConnack->remainingLength;
    /*Deserializing Connack packet received from the netwok*/
    mqttStatus = MQTT_DeserializeAck( &pIncomingPacket, &( pConnack->packetIdentifier ), &sessionPresent );
    status = convertReturnCode( mqttStatus );
    return status;
}

/*-----------------------------------------------------------*/

/* Deserializer Suback wrapper .*/
IotMqttError_t _deserializeSubackWrapper( _mqttPacket_t * pSuback )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;

    /* Null Check for suback packet  */
    IotMqtt_Assert( pSuback != NULL );

    pIncomingPacket.type = pSuback->type;
    pIncomingPacket.pRemainingData = pSuback->pRemainingData;
    pIncomingPacket.remainingLength = pSuback->remainingLength;
    /*Deserializing SUBACK packet received from the netwok*/
    mqttStatus = MQTT_DeserializeAck( &pIncomingPacket, &( pSuback->packetIdentifier ), NULL );
    status = convertReturnCode( mqttStatus );
    return status;
}

/*-----------------------------------------------------------*/

/* Deserializer Unsuback wrapper .*/
IotMqttError_t _deserializeUnsubackWrapper( _mqttPacket_t * pUnsuback )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;

    /* Null Check for unsuback packet  */
    IotMqtt_Assert( pUnsuback != NULL );

    pIncomingPacket.type = pUnsuback->type;
    pIncomingPacket.pRemainingData = pUnsuback->pRemainingData;
    pIncomingPacket.remainingLength = pUnsuback->remainingLength;
    /*Deserializing UNSUBACK packet received from the netwok*/
    mqttStatus = MQTT_DeserializeAck( &pIncomingPacket, &( pUnsuback->packetIdentifier ), NULL );
    status = convertReturnCode( mqttStatus );

    return status;
}

/*-----------------------------------------------------------*/

/* Deserializer Puback wrapper .*/
IotMqttError_t _deserializePubackWrapper( _mqttPacket_t * pPuback )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;

    /* Null Check for puback packet  */
    IotMqtt_Assert( pPuback != NULL );

    pIncomingPacket.type = pPuback->type;
    pIncomingPacket.pRemainingData = pPuback->pRemainingData;
    pIncomingPacket.remainingLength = pPuback->remainingLength;
    /*Deserializing PUBACK packet received from the netwok*/
    mqttStatus = MQTT_DeserializeAck( &pIncomingPacket, &( pPuback->packetIdentifier ), NULL );
    status = convertReturnCode( mqttStatus );
    return status;
}

/*-----------------------------------------------------------*/

/* Deserializer Ping Response wrapper .*/
IotMqttError_t _deserializePingrespWrapper( _mqttPacket_t * pPingresp )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;

    /* Null Check for Pingresponse packet  */
    IotMqtt_Assert( pPingresp != NULL );

    pIncomingPacket.type = pPingresp->type;
    pIncomingPacket.pRemainingData = pPingresp->pRemainingData;
    pIncomingPacket.remainingLength = pPingresp->remainingLength;
    /*Deserializing PINGRESP packet received from the netwok*/
    mqttStatus = MQTT_DeserializeAck( &pIncomingPacket, &( pPingresp->packetIdentifier ), NULL );
    status = convertReturnCode( mqttStatus );
    return status;
}

/*-----------------------------------------------------------*/

/* Deserializer Publish wrapper .*/
IotMqttError_t _deserializePublishWrapper( _mqttPacket_t * pPublish )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTPacketInfo_t pIncomingPacket;
    MQTTPublishInfo_t publishInfo;

    /* Null Check for Publish packet  */
    IotMqtt_Assert( pPublish != NULL );

    pIncomingPacket.type = pPublish->type;
    pIncomingPacket.pRemainingData = pPublish->pRemainingData;
    pIncomingPacket.remainingLength = pPublish->remainingLength;


    /*DeSerializing publish packet received from the netwok*/
    mqttStatus = MQTT_DeserializePublish( &pIncomingPacket, &( pPublish->packetIdentifier ), &publishInfo );

    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        pPublish->u.pIncomingPublish->u.publish.publishInfo.qos = ( IotMqttQos_t ) publishInfo.qos;
        pPublish->u.pIncomingPublish->u.publish.publishInfo.payloadLength = publishInfo.payloadLength;
        pPublish->u.pIncomingPublish->u.publish.publishInfo.pPayload = publishInfo.pPayload;
        pPublish->u.pIncomingPublish->u.publish.publishInfo.pTopicName = publishInfo.pTopicName;
        pPublish->u.pIncomingPublish->u.publish.publishInfo.topicNameLength = publishInfo.topicNameLength;
        pPublish->u.pIncomingPublish->u.publish.publishInfo.retain = publishInfo.retain;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Suback Serializer Wrapper .*/
IotMqttError_t _pubackSerializeWrapper( uint16_t packetIdentifier,
                                        uint8_t ** pPubackPacket,
                                        size_t * pPacketSize )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    MQTTFixedBuffer_t networkBuffer;
    uint8_t packetTypeByte = MQTT_PACKET_TYPE_PUBACK;

    /* Initializing network buffer*/
    networkBuffer.pBuffer = IotMqtt_MallocMessage( MQTT_PACKET_PUBACK_SIZE );
    networkBuffer.size = MQTT_PACKET_PUBACK_SIZE;
    *pPacketSize = MQTT_PACKET_PUBACK_SIZE;

    /*Serializing puback packet to be sent on the netwok*/
    status = MQTT_SerializeAck( &( networkBuffer ),
                                packetTypeByte,
                                packetIdentifier );
    status = convertReturnCode( mqttStatus );

    if( status == IOT_MQTT_SUCCESS )
    {
        *pPubackPacket = networkBuffer.pBuffer;
    }

    return status;
}

/*-----------------------------------------------------------*/

/* Convert the MQTT Status to IOT MQTT Status Code */
IotMqttError_t convertReturnCode( MQTTStatus_t mqttStatus )
{
    IotMqttError_t status = IOT_MQTT_SUCCESS;

    switch( mqttStatus )
    {
        case MQTTSuccess:
            status = IOT_MQTT_SUCCESS;
            break;

        case MQTTBadParameter:
            status = IOT_MQTT_BAD_PARAMETER;
            break;

        case MQTTNoMemory:
            status = IOT_MQTT_NO_MEMORY;
            break;

        case MQTTSendFailed:
        case MQTTRecvFailed:
            status = IOT_MQTT_NETWORK_ERROR;
            break;

        case MQTTBadResponse:
            status = IOT_MQTT_BAD_RESPONSE;
            break;

        case MQTTServerRefused:
            status = IOT_MQTT_SERVER_REFUSED;
            break;

        case MQTTNoDataAvailable:
        case MQTTKeepAliveTimeout:
            status = IOT_MQTT_TIMEOUT;

        case MQTTIllegalState:
        case MQTTStateCollision:
            status = IOT_MQTT_BAD_RESPONSE;

        default:
            status = IOT_MQTT_SUCCESS;
            break;
    }

    return status;
}

/*-----------------------------------------------------------*/
