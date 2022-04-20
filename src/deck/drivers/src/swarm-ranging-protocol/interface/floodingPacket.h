#ifndef __FLOODINGPACKET_H__
#define __FLOODINGPACKET_H__

#include <stdint.h>

#define F_MESSAGE_PAYLOAD_MAX_SIZE 9 // 计算

// Flooding message
typedef struct
{
    uint16_t originatorAddress;
    uint16_t sequence;
    uint16_t size;
    uint8_t timeToLive;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} __attribute__((packed)) fMessageHeader_t; // 7 bytes

typedef struct
{
    uint16_t originatorAddress;
    uint16_t destinationAddress;
    uint16_t sequence; //不需要
    uint16_t distance;
} __attribute__((packed)) fMessagePayloadUnit_t; // 8 bytes

typedef struct
{
    fMessageHeader_t fMessageHeader;
    fMessagePayloadUnit_t fMessagePayload[F_MESSAGE_PAYLOAD_MAX_SIZE];
} __attribute__((packed)) fMessage_t;


#endif // __FLOODINGPACKET_H__