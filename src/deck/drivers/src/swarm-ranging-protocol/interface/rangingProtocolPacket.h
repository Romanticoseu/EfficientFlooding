#ifndef __RANGINGPROTOCOLPACKET_H__
#define __RANGINGPROTOCOLPACKET_H__

#include "rangingProtocolStruct.h"

/*

*/
#define PACKET_MAX_LENGTH 125 // got this value by testing
#define PACKET_PALOAD_MAX_SIZE 104 // (PACKET_MAX_LENGTH - MAC802154_HEADER_LENGTH)
#define MESSAGE_MAX_LENGTH 100 // (MESSAGE_MAX_LENGTH = PACKET_PALOAD_MAX_SIZE), but here, we set it 100
#define MESSAGE_PAYLOAD_MAX_SIZE 98 // (MESSAGE_MAX_LENGTH - sizeof(messageHeader_t))
#define TS_MESSAGE_PAYLOAD_MAX_SIZE 9 // (TS_MESSAGE_PAYLOAD_MAX_SIZE = RANGING_TABLE_ITEM_SET_SIZE)

// message header
// message header struct
typedef struct
{
    uint16_t messageLength; // 2bytes
} __attribute__((packed)) messageHeader_t; // 2bytes
// message struct
typedef struct
{
    messageHeader_t messageHeader; // 2bytes
    uint8_t messagePayload[MESSAGE_PAYLOAD_MAX_SIZE]; // 98bytes
} __attribute__((packed)) message_t;

// TS message
// TS message header struct
typedef struct
{
    uint16_t lastTxSequence; // 2bytes
    uint16_t messageSize; // 2bytes
    uint16_t originatorAddress; // 2bytes
    uint32_t dwTimeLow32; // 4bytes
    uint8_t dwTimeHigh8; // 1bytes
    uint16_t velocity; // 2bytes, and its unit is cm
    uint16_t messageSequence; // 2bytes
} __attribute__((packed)) tsMessageHeader_t; // 15bytes
// TS message payload unit struct
typedef struct
{
    uint16_t originatorAddress; // 2bytes
    uint16_t messageSequence; // 2bytes
    uint8_t dwTimeHigh8; // 1bytes
    uint32_t dwTimeLow32; // 4bytes
} __attribute__((packed)) tsMessagePayloadUnit_t; // 9bytes
// TS message struct
typedef struct
{
    tsMessageHeader_t tsMessageHeader;
    tsMessagePayloadUnit_t tsMessagePayload[TS_MESSAGE_PAYLOAD_MAX_SIZE];
} __attribute__((packed)) tsMessage_t;

#endif // __RANGINGPROTOCOLPACKET_H__
