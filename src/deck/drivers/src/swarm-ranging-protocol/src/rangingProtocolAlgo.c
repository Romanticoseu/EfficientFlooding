#define DEBUG_MODULE "TS"

#include <stdint.h>
#include <stdlib.h>

#include "log.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "rangingProtocolAlgo.h"
#include "rangingProtocolDebug.h"
#include "rangingProtocolStruct.h"
#include "rangingProtocolPacket.h"
#include "param.h"
tsTimestampTuple_t tsTxPool[TS_TX_POOL_MAXSIZE] = {0};

static int16_t distanceTowards[50];

static int tsTxPoolIndex = 0;
static int tsReceiveCount = 0;
static int tsReceiveErrorCount = 0;
static int tsComputeError = 0;
static float tsVelocity;
uint16_t distancetest;
uint16_t temaddress;
uint8_t type=0;
uint8_t tx_red;
uint8_t tx_green;
uint8_t tx_blue;
void GenerateTs(uint16_t tsMessageSeq, float velocity, uint16_t myAddress, tsTime_t *tsNextSendTime, packet_t *txTsPacket)
{
    // DEBUG_PRINT_ALGO("START GENERATE TS\n");
    // jitter and velocity
    int jitter = 0;
    tsVelocity = velocity;
    // ranging table solving
    TsRangingTableClearExpire(&tsRangingTable);
    TsSortRangingTable(&tsRangingTable);
    // TsPrintRangingTable(&tsRangingTable);
    // generate TS message header
    message_t *tsMessage = (message_t *) txTsPacket->payload;
    // 修改
    tsMessageHeader_t *tsMessageHeader = (tsMessageHeader_t *) tsMessage->messagePayload;
    tsTimestampTuple_t *tsTx = &tsTxPool[tsTxPoolIndex];
    tsMessageHeader->messageSize = sizeof(tsMessageHeader_t);
    tsMessageHeader->originatorAddress = myAddress;
    tsMessageHeader->messageSequence = tsMessageSeq;
    tsMessageHeader->dwTimeHigh8 = tsTx->timestamp.high8;
    tsMessageHeader->dwTimeLow32 = tsTx->timestamp.low32;
    tsMessageHeader->lastTxSequence = tsTx->sequenceNumber; 
    tsMessageHeader->velocity = velocity;

    // DEBUG_PRINT_ALGO_DATA("TX MESSAGE SIZE: %u\n", tsMessageHeader->messageSize);
    // DEBUG_PRINT_ALGO_DATA("TX ORIGINATOR ADDRESS: %u\n", tsMessageHeader->originatorAddress);
    // DEBUG_PRINT_ALGO_DATA("TX MESSAGE SEQENCE: %u\n", tsMessageHeader->messageSequence);
    // DEBUG_PRINT_ALGO_DATA("TX TIMESTAMP: %llu\n", tsTx->timestamp.full);
    // DEBUG_PRINT_ALGO_DATA("TX LAST SEQENCE: %u\n", tsTx->sequenceNumber);
    // DEBUG_PRINT_ALGO_DATA("TX VELOCITY: %u\n", tsMessageHeader->velocity);
    // generate TS message payload
    uint8_t *tsMessagePayloadStart = (uint8_t *) tsMessage->messagePayload + sizeof(tsMessageHeader_t);
    uint8_t *tsMessagePayloadEnd = (uint8_t *) tsMessage->messagePayload + MESSAGE_MAX_LENGTH;
    tsMessagePayloadUnit_t *tsMessagePayloadUnit = (tsMessagePayloadUnit_t *) tsMessagePayloadStart;
    for(itemIndex_t index = tsRangingTable.fullQueueEntry; index != -1; index = tsRangingTable.itemSet[index].next)
    {
        tsRangingTableItem_t *rangingTableItem = &tsRangingTable.itemSet[index];
        if((uint8_t *)(tsMessagePayloadUnit + 1) > tsMessagePayloadEnd)
        {
            break;
        }
        if(rangingTableItem->rangingTuple.nextDeliveryTime <= xTaskGetTickCount() + TS_INTERVAL_MIN && rangingTableItem->rangingTuple.Re.timestamp.full)
        {
            tsMessagePayloadUnit->originatorAddress = rangingTableItem->rangingTuple.peerAddress;
            tsMessagePayloadUnit->messageSequence = rangingTableItem->rangingTuple.Re.sequenceNumber;
            tsMessagePayloadUnit->dwTimeHigh8 = rangingTableItem->rangingTuple.Re.timestamp.high8;
            tsMessagePayloadUnit->dwTimeLow32 = rangingTableItem->rangingTuple.Re.timestamp.low32;
            tsMessagePayloadUnit++;
            tsMessageHeader->messageSize += sizeof(tsMessagePayloadUnit_t);
            rangingTableItem->rangingTuple.Re.sequenceNumber = 0;
            rangingTableItem->rangingTuple.Re.timestamp.full = 0;
        }
        jitter = (int)(rand()/(float)RAND_MAX * 9) - 4;
        rangingTableItem->rangingTuple.nextDeliveryTime = xTaskGetTickCount() + rangingTableItem->rangingTuple.period + jitter;
        if(rangingTableItem->rangingTuple.nextDeliveryTime < *tsNextSendTime)
        {
            *tsNextSendTime = rangingTableItem->rangingTuple.nextDeliveryTime;
        }
    }
    // DEBUG_PRINT_ALGO_DATA("TX MESSAGE SEQENCE: %u\n", tsMessageHeader->messageSequence);
    tsMessage->messageHeader.messageLength = sizeof(messageHeader_t) + tsMessageHeader->messageSize;
}

void UpdateTxTs(dwTime_t txTimestamp, uint16_t packetSeq)
{
    // DEBUG_PRINT_ALGO("START UPDATE TX TS\n");

    tsTxPoolIndex++;
    tsTxPoolIndex %= TS_TX_POOL_MAXSIZE;
    tsTxPool[tsTxPoolIndex].sequenceNumber = packetSeq;
    tsTxPool[tsTxPoolIndex].timestamp = txTimestamp;

    // DEBUG_PRINT_ALGO_DATA("last seq: %u\n", tsTxPool[tsTxPoolIndex].sequenceNumber);
}

int16_t TsComputeDistance(tsRangingTuple_t *rangingTuple)
{
    // DEBUG_PRINT_ALGO("START TS COMPUTE DISTANCE\n");
    int64_t ad, bp, bd, ap, tp;
    bool isErrorOccurred = false;

    ad = (rangingTuple->Rr.timestamp.full - rangingTuple->Tp.timestamp.full + MAX_TIMESTAMP) % MAX_TIMESTAMP;
    bp = (rangingTuple->Tr.timestamp.full - rangingTuple->Rp.timestamp.full + MAX_TIMESTAMP) % MAX_TIMESTAMP;
    bd = (rangingTuple->Rf.timestamp.full - rangingTuple->Tr.timestamp.full + MAX_TIMESTAMP) % MAX_TIMESTAMP;
    ap = (rangingTuple->Tf.timestamp.full - rangingTuple->Rr.timestamp.full + MAX_TIMESTAMP) % MAX_TIMESTAMP;
    tp = ((ad * bd) - (ap * bp))/(ad + bd + ap + bp);

    if(tp < -100 || tp > 900)
    {
        tsComputeError++;
        isErrorOccurred = true;
    }

    // update
    if(bd < 0 || ap < 0)
    {
        rangingTuple->Rf.timestamp.full = 0;
        rangingTuple->Tf.timestamp.full = 0;
        return -0;
    }
    rangingTuple->Rp = rangingTuple->Rf;
    rangingTuple->Rf.timestamp.full = 0;
    rangingTuple->Tp = rangingTuple->Tf;
    rangingTuple->Tf.timestamp.full = 0;
    rangingTuple->Rr = rangingTuple->Re;
    rangingTuple->Tr.timestamp.full = 0;
    if(isErrorOccurred){
        return rangingTuple->distance;
    }
    return (int16_t) tp * 0.4691763978616;
}

void UpdateRxTs(const message_t* message, const tsTimestampTuple_t *rxTimestamp, uint16_t myAddress)
{
    // DEBUG_PRINT_ALGO("START UPDATE RX TS\n");
    // DEBUG_PRINT_ALGO_DATA("messageLength: %u\n", message->messageHeader.messageLength);
    // tsMessage_t *testTsMessage = (tsMessage_t *) message->messagePayload;
    // DEBUG_PRINT_ALGO_DATA("lastTxSequence: %u\n", testTsMessage->tsMessageHeader.lastTxSequence);
    // DEBUG_PRINT_ALGO_DATA("messageSequence: %u\n", testTsMessage->tsMessageHeader.messageSequence);
    // DEBUG_PRINT_ALGO_DATA("messageSize: %u\n", testTsMessage->tsMessageHeader.messageSize);
    // DEBUG_PRINT_ALGO_DATA("originatorAddress: %u\n", testTsMessage->tsMessageHeader.originatorAddress);
    //若收到的数据包的时间戳小于最新发送的数据包的时间戳，则收到的数据包已过期
    if(rxTimestamp->timestamp.full < tsTxPool[tsTxPoolIndex].timestamp.full)
    {
        // DEBUG_PRINT_ALGO_DATA("ERROR\n");
        tsReceiveErrorCount++;
        return;
    }
    tsReceiveCount++;
    //测距数据包头部分数据提取
    tsMessageHeader_t *tsMessageHeader = (tsMessageHeader_t *) message->messagePayload;
    tsAddress_t peerAddress = tsMessageHeader->originatorAddress;
    // uint16_t peerSpeed = tsMessageHeader->velocity;
    tsTimestampTuple_t peerTxTimestamp = {0};
    peerTxTimestamp.sequenceNumber = tsMessageHeader->lastTxSequence;
    peerTxTimestamp.timestamp.low32 = tsMessageHeader->dwTimeLow32;
    peerTxTimestamp.timestamp.high8 = tsMessageHeader->dwTimeHigh8;
    // DEBUG_PRINT_ALGO_DATA("PEER ADDRESS: %u\n", peerAddress);
    // DEBUG_PRINT_ALGO_DATA("PEER LAST TX SEQUENCE: %u\n", peerTxTimestamp.sequenceNumber);
    // DEBUG_PRINT_ALGO_DATA("PEER MESSAGE SEQUENCE: %u\n", tsMessageHeader->messageSequence);
    // DEBUG_PRINT_ALGO_DATA("PEER MESSAGE SIZE: %u\n", tsMessageHeader->messageSize);
    // DEBUG_PRINT_ALGO_DATA("PEER TX TIMESTAMP: %llu\n", peerTxTimestamp.timestamp.full);
    //测距数据包体部分数据提取
    tsTimestampTuple_t peerRxTimestamp = {0};
    uint8_t *tsMessagePayloadStart = (uint8_t *) message->messagePayload + sizeof(tsMessageHeader_t);
    uint8_t *tsMessagePayloadEnd = (uint8_t *) message->messagePayload + tsMessageHeader->messageSize;
    //寻找与本机ID对应的数据体
    for(tsMessagePayloadUnit_t *tsMessagePayloadUnit = (tsMessagePayloadUnit_t *) tsMessagePayloadStart; (uint8_t *) tsMessagePayloadUnit < tsMessagePayloadEnd; tsMessagePayloadUnit++)
    {
        if(tsMessagePayloadUnit->originatorAddress != myAddress) continue;

        peerRxTimestamp.sequenceNumber = tsMessagePayloadUnit->messageSequence;
        //DEBUG_PRINT_ALGO_DATA("PEER RX SEQ: %u\n", peerRxTimestamp.sequenceNumber);
        peerRxTimestamp.timestamp.low32 = tsMessagePayloadUnit->dwTimeLow32;
        peerRxTimestamp.timestamp.high8 = tsMessagePayloadUnit->dwTimeHigh8;
        
        break;
    }
    //对rangingTable的填写与修改
    itemIndex_t peerIndex = TsFindInRangingTable(&tsRangingTable, peerAddress);
    //若该测距表格节点没有初始化，则初始化
    tsRangingTuple_t rangingTuple01;
    if(peerIndex == -1)
    {
        
        memset(&rangingTuple01, 0, sizeof(tsRangingTuple_t));
        rangingTuple01.peerAddress = peerAddress;
        rangingTuple01.period = M2T(TS_INTERVAL);
        rangingTuple01.nextDeliveryTime = xTaskGetTickCount() + rangingTuple01.period;
        rangingTuple01.expiration = xTaskGetTickCount() + M2T(RANGING_TABLE_HOLD_TIME);
        peerIndex = TsRangingTableInsert(&tsRangingTable, &rangingTuple01);
        // DEBUG_PRINT_ALGO_DATA("peer index is: %d\n", peerIndex);
        if(peerIndex == -1)
        {
            //无人机数量超过上限
            return;
        }
    }
    // 获取rangingTable中对方的节点
    tsRangingTuple_t *rangingTuple02 = &tsRangingTable.itemSet[peerIndex].rangingTuple;
    rangingTuple02->expiration = xTaskGetTickCount() + M2T(RANGING_TABLE_HOLD_TIME);
    // 开始测距表格更新
    // 更新 Re
    rangingTuple02->Re = *rxTimestamp;
    // 错误防范机制
    // 更新 Tr 或 Rr, 错误情况case4, 以及最初消息传递的情况
    if(rangingTuple02->Tr.timestamp.full == 0)
    {
        if(rangingTuple02->Rr.sequenceNumber == peerTxTimestamp.sequenceNumber)
        {
            rangingTuple02->Tr = peerTxTimestamp;
        }
        else
        {
            rangingTuple02->Rr = *rxTimestamp;
        }
    }
    // 更新 Rf 和 Tf 如果 peerRxTimestamp 有值, 错误情况case1、case2
    if(peerRxTimestamp.timestamp.full)
    {
        rangingTuple02->Rf = peerRxTimestamp;
        for(int i = 0; i < TS_TX_POOL_MAXSIZE; i++)
        {
            //DEBUG_PRINT_ALGO_DATA("enter\n");
            if(tsTxPool[i].sequenceNumber == peerRxTimestamp.sequenceNumber)
            {
                rangingTuple02->Tf = tsTxPool[i];
                break;
            }
        }
    }
    // 计算距离或者什么都不做只是将表重新组织
    if(rangingTuple02->Tr.timestamp.full && rangingTuple02->Rf.timestamp.full && rangingTuple02->Tf.timestamp.full)
    {
        rangingTuple02->distance = TsComputeDistance(rangingTuple02);
        distancetest=rangingTuple02->distance;
        distanceTowards[rangingTuple02->peerAddress] = rangingTuple02->distance;
        if(rangingTuple02->distance > 1000 || rangingTuple02->distance < -100)
        {
            // DEBUG_PRINT_ALGO("TS COMPUTE DISTANCE ERROR\n");
            // DEBUG_PRINT_ALGO_DATA("compute error\n");
        }
        else
        {
            // DEBUG_PRINT_ALGO("TS COMPUTE DISTANCE SUCCESS\n");
            // DEBUG_PRINT_ALGO_DATA("compute: %d\n", rangingTuple02->distance);
        }
        if(rangingTuple02->distance<80)
        {
            tx_red=0;
            tx_blue=0;
            tx_green=0;
            if(rangingTuple02->peerAddress!=temaddress)
            {
                type=(type+1)%3;
                temaddress=rangingTuple02->peerAddress;
            }
            if(type==0)
            {
                tx_red=255-3*rangingTuple02->distance;
                tx_blue=0;
                tx_green=0;
            }
            else if(type==1)
            {
                tx_red=0;
                tx_blue=255-3*rangingTuple02->distance;
                tx_green=0;
            }
            else if(type==2)
            {
                tx_red=0;
                tx_blue=0;
                tx_green=255-3*rangingTuple02->distance;
            }

            
        }
        

        

       
    }
    else if(rangingTuple02->Rf.timestamp.full && rangingTuple02->Tf.timestamp.full)
    {
        rangingTuple02->Rp = rangingTuple02->Rf;
        rangingTuple02->Rf.timestamp.full = 0;
        rangingTuple02->Tp = rangingTuple02->Tf;
        rangingTuple02->Tf.timestamp.full = 0;
        rangingTuple02->Rr = rangingTuple02->Re;
        rangingTuple02->Tr.timestamp.full = 0;
    }
    

}

// log define
LOG_GROUP_START(TSranging)
        LOG_ADD(LOG_INT16, distTo1, distanceTowards+1)
        LOG_ADD(LOG_INT16, distTo2, distanceTowards+2)
        LOG_ADD(LOG_INT16, distTo3, distanceTowards+3)
        LOG_ADD(LOG_INT16, distTo4, distanceTowards+4)
        LOG_ADD(LOG_INT16, distTo5, distanceTowards+5)
        LOG_ADD(LOG_INT16, distTo6, distanceTowards+6)
        LOG_ADD(LOG_INT16, distTo7, distanceTowards+7)
        LOG_ADD(LOG_INT16, distTo8, distanceTowards+8)
        LOG_ADD(LOG_INT16, distTo9, distanceTowards+9)
        LOG_ADD(LOG_FLOAT, velocity, &tsVelocity)
        LOG_ADD(LOG_INT16, total, &tsReceiveCount)
        LOG_ADD(LOG_INT16, error, &tsReceiveErrorCount)
        LOG_ADD(LOG_INT16, computeError, &tsComputeError )
LOG_GROUP_STOP(TSranging)