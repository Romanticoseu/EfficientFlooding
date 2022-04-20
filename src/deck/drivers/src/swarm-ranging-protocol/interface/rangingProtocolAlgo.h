#ifndef __RANGINGPROTOCOLALGO_H__
#define __RANGINGPROTOCOLALGO_H__

#include "rangingProtocolStruct.h"
#include "rangingProtocolPacket.h"

#define TS_INTERVAL 200
#define TS_INTERVAL_MIN 20 
#define TS_INTERVAL_MAX 500 
#define TS_TX_POOL_MAXSIZE (4*TS_INTERVAL_MAX/TS_INTERVAL)
#define RANGING_TABLE_HOLD_TIME 10000
#define MAX_TIMESTAMP 1099511627776 //2**40

void GenerateTs(uint16_t tsMessageSeq, float velocity, uint16_t myAddress, tsTime_t *tsNextSendTime, packet_t *txTsPacket);
void UpdateTxTs(dwTime_t txTimestamp, uint16_t packetSeq);
void UpdateRxTs(const message_t* tsMessage, const tsTimestampTuple_t *rxTimestamp, uint16_t myAddress);

#endif // __RANGINGPROTOCOLALGO_H__