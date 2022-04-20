#ifndef __RANGINGPROTOCOLSTRUCT_H__
#define __RANGINGPROTOCOLSTRUCT_H__

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "mac.h"

#include "adhocdeck.h"

#define RANGING_TABLE_ITEM_SET_SIZE 10

typedef TickType_t tsTime_t;
typedef uint16_t tsAddress_t;
typedef short itemIndex_t;

typedef struct
{
    uint16_t sequenceNumber;
    dwTime_t timestamp;
} tsTimestampTuple_t;

typedef struct {
  packet_t packet;
  tsTimestampTuple_t rxTimestamp;
}tsPacketWithTimestamp_t;

typedef struct
{
    tsAddress_t peerAddress;
    // timestamps from dw1000
    tsTimestampTuple_t Rp;
    tsTimestampTuple_t Tp;
    tsTimestampTuple_t Rr;
    tsTimestampTuple_t Tr;
    tsTimestampTuple_t Rf;
    tsTimestampTuple_t Tf;
    tsTimestampTuple_t Re;
    // tick from stm32-FreeRTOS
    tsTime_t period;
    tsTime_t nextDeliveryTime;
    tsTime_t expiration;
    int16_t distance;
} tsRangingTuple_t;

typedef struct
{
    tsRangingTuple_t rangingTuple;
    itemIndex_t next;
} tsRangingTableItem_t;

typedef struct
{
    tsRangingTableItem_t itemSet[RANGING_TABLE_ITEM_SET_SIZE];
    itemIndex_t freeQueueEntry;
    itemIndex_t fullQueueEntry;
    int size;
} tsRangingTable_t;

tsRangingTable_t tsRangingTable;

void TsRangingTableInit(tsRangingTable_t *rangingTable);
bool TsRangingTableClearExpire(tsRangingTable_t *rangingTable);
void TsSortRangingTable(tsRangingTable_t *rangingTable);
void TsPrintRangingTable(tsRangingTable_t *rangingTable);
itemIndex_t TsFindInRangingTable(tsRangingTable_t *rangingTable, tsAddress_t address);
itemIndex_t TsRangingTableInsert(tsRangingTable_t *rangingTable, tsRangingTuple_t *tuple);

#endif // __RANGINGPROTOCOLSTRUCT_H__