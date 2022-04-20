#include <stdint.h>

#include "rangingProtocolPacket.h"
#include "rangingProtocolStruct.h"
#include "floodingAlgo.h"
#include "floodingPacket.h"
#include "floodingStruct.h"
#include "rangingProtocolDebug.h"


static int fMessageSeq = 0;
static int fMessageTopologySeq = 0;
void GenerateF(uint16_t myAddress, packet_t *txFPacket, uint8_t timeToLive,uint8_t red,uint8_t green,uint8_t blue)
{
    DEBUG_PRINT_ALGO("START GENERATE F\n");
    //泛洪消息报文头生成
    message_t *message = (message_t *) txFPacket->payload;
    fMessageHeader_t *fMessageHeader = (fMessageHeader_t *) message->messagePayload;
    fMessageHeader->originatorAddress = myAddress;
    fMessageHeader->sequence = fMessageSeq;
    fMessageHeader->size = sizeof(fMessageHeader_t);
    fMessageHeader->timeToLive = timeToLive;
    fMessageHeader->red=red;
    fMessageHeader->green=green;
    fMessageHeader->blue=blue;
    // DEBUG_PRINT_ALGO_DATA("F MESSAGE ORIGINATORADDRESS: %u\n", fMessageHeader->originatorAddress);
    // DEBUG_PRINT_ALGO_DATA("F MESSAGE SEQUENCE: %u\n", fMessageHeader->sequence);
    // DEBUG_PRINT_ALGO_DATA("F MESSAGE SIZE: %u\n", fMessageHeader->size);
    // DEBUG_PRINT_ALGO_DATA("F MESSAGE TIMETOLIVE: %u\n", fMessageHeader->timeToLive);
    // 泛洪消息报文负载装载
    uint8_t *fMessagePayloadStart = (uint8_t *) message->messagePayload + sizeof(fMessageHeader_t);
    uint8_t *fMessagePayloadEnd = (uint8_t *) message->messagePayload + MESSAGE_MAX_LENGTH;
    fMessagePayloadUnit_t *fMessagePayloadUnit = (fMessagePayloadUnit_t *) fMessagePayloadStart;
    fTopologyTableTuple_t fTopologyTableTuple;
    // 如果测距表内没有与某无人机的信息，则泛洪消息也不会装载与该无人机相关的信息
    for (index_t index = tsRangingTable.fullQueueEntry; index != -1; index = tsRangingTable.itemSet[index].next)
    {
        tsRangingTableItem_t *rangingTableItem = &tsRangingTable.itemSet[index];
        if ((uint8_t *)(fMessagePayloadUnit + 1) > fMessagePayloadEnd)
        {
            break;
        }
        // 进行逐个装载
        fMessagePayloadUnit->originatorAddress = myAddress;
        fMessagePayloadUnit->destinationAddress = rangingTableItem->rangingTuple.peerAddress;
        fMessagePayloadUnit->sequence = fMessageTopologySeq;
        fMessagePayloadUnit->distance = rangingTableItem->rangingTuple.distance;
        // DEBUG_PRINT_ALGO_DATA("F MESSAGE ORIGINATORADDRESS: %u\n", fMessagePayloadUnit->originatorAddress);
        // DEBUG_PRINT_ALGO_DATA("F MESSAGE DESTINATIONADDRESS: %u\n", fMessagePayloadUnit->destinationAddress);
        // DEBUG_PRINT_ALGO_DATA("F MESSAGE SEQUENCE: %u\n", fMessagePayloadUnit->sequence);
        // DEBUG_PRINT_ALGO_DATA("F MESSAGE DISTANCE: %u\n", fMessagePayloadUnit->distance);
        // 更新泛洪拓扑表
        fTopologyTableTuple.originatorAddress = myAddress;
        fTopologyTableTuple.destinationAddress = rangingTableItem->rangingTuple.peerAddress;
        fTopologyTableTuple.distance = rangingTableItem->rangingTuple.distance;
        index_t topologyTableItemIndex = FFindInTopologyTable(&fTopologyTable, myAddress, rangingTableItem->rangingTuple.peerAddress);
        // 没有记录，进行插入
        if (topologyTableItemIndex== -1)
        {
            index_t candidate = FTopologyTableInsert(&fTopologyTable, &fTopologyTableTuple);
            if (candidate == -1)
            { 
                // 插入泛洪拓扑表失败
            }
        }
        // 有记录，进行更新
        else
        {
            fTopologyTableTuple_t *topologyTuple = &fTopologyTable.fTopologyTableItemSet[topologyTableItemIndex].fTopologyTableTuple;
            topologyTuple->distance = fTopologyTableTuple.distance;
        }
        // 更新数据
        fMessagePayloadUnit++;
        fMessageTopologySeq++;
        fMessageHeader->size += sizeof(fMessagePayloadUnit_t);
    }

    // 数据更新
    // DEBUG_PRINT_ALGO_DATA("F MESSAGE SIZE: %u\n", fMessageHeader->size);
    fMessageSeq++;
    message->messageHeader.messageLength = sizeof(messageHeader_t) + fMessageHeader->size;
}

bool CheckRxF(const message_t* message, uint16_t myAddress)
{
    DEBUG_PRINT_ALGO("START CHECK RX F\n");
    // 解析报文
    fMessageHeader_t *fMessageHeader = (fMessageHeader_t *) message->messagePayload;
    // 如果原地址是自己则直接返回
    if (fMessageHeader->originatorAddress == myAddress)
    {
        return false;
    }
    // 检查TTL信息，若为0则直接返回，反之则继续
    if (fMessageHeader->timeToLive == 0)
    {
        return false;
    }
    else
    {
        fMessageHeader->timeToLive--;
    }
    // 在泛洪检查表中检索源地址序列，若candidate为-1则无记录，反之有记录
    index_t candidate = FFindInCheckTable(&fCheckTable, fMessageHeader->originatorAddress);
    // DEBUG_PRINT_ALGO_DATA("F TABLE CANDIDATE: %d\n", candidate);
    // 报文重复检查
    // candidate为-1，在泛洪检查表中插入记录，并转发
    if(candidate == -1)
    {
        // TODO: 处理泛洪检查表空间已满情况
        candidate = FCheckTableInsert(&fCheckTable, fMessageHeader->originatorAddress, fMessageHeader->sequence);
        return true;
    }
    // candidate不为-1，检查序列号是否重复
    else
    {
        // 如果序列号相等或小于，则不转发，返回false
        if(fMessageHeader->sequence <= fCheckTable.fCheckTableItemSet[candidate].sequence)
        {
            return false;
        }
        // 反之，需要转发，并更新序列号
        else
        {
            return true;
            fCheckTable.fCheckTableItemSet[candidate].sequence = fMessageHeader->sequence;
        }
    }
}

void UpdateRxF(const message_t* message)
{
    DEBUG_PRINT_ALGO("START UPDATE RX F\n");
    // 泛洪消息解析，提取负载部分
    fMessageHeader_t *fMessageHeader = (fMessageHeader_t *) message->messagePayload;
    uint8_t *fMessagePayloadStart = (uint8_t *) message->messagePayload + sizeof(fMessageHeader_t);
    uint8_t *fMessagePayloadEnd = (uint8_t *) message->messagePayload + fMessageHeader->size;
    // 报文负载数据提取
    fTopologyTableTuple_t fTopologyTableTuple;
    for (fMessagePayloadUnit_t *fMessagePayloadUnit = (fMessagePayloadUnit_t *) fMessagePayloadStart; (uint8_t *) fMessagePayloadUnit < fMessagePayloadEnd; fMessagePayloadUnit++)
    {
        fTopologyTableTuple.originatorAddress = fMessagePayloadUnit->originatorAddress;
        fTopologyTableTuple.destinationAddress = fMessagePayloadUnit->destinationAddress;
        fTopologyTableTuple.distance = fMessagePayloadUnit->distance;
        // DEBUG_PRINT_ALGO_DATA("F TABLE ORIGINATORADDRESS: %u\n", fTopologyTableTuple.originatorAddress);
        // DEBUG_PRINT_ALGO_DATA("F TABLE DESTINATIONADDRESS: %u\n", fTopologyTableTuple.destinationAddress);
        // DEBUG_PRINT_ALGO_DATA("F TABLE SEQUENCE: %u\n", fTopologyTableTuple.sequence);
        // DEBUG_PRINT_ALGO_DATA("F TABLE DISTANCE: %u\n", fTopologyTableTuple.distance);
        index_t topologyTableItemIndex = FFindInTopologyTable(&fTopologyTable, fMessagePayloadUnit->originatorAddress, fMessagePayloadUnit->destinationAddress);
        // DEBUG_PRINT_ALGO_DATA("F TABLE INDEX: %d\n", topologyTableItemIndex);
        // 如果无记录，则初始化
        if (topologyTableItemIndex == -1)
        {
            index_t candidate = FTopologyTableInsert(&fTopologyTable, &fTopologyTableTuple);
            if (candidate == -1)
            { 
                // 插入泛洪拓扑表失败
            }
        }
        // 如果有记录且记录的序列号为最新，则更新记录
        else
        {
            fTopologyTableTuple_t *topologyTuple = &fTopologyTable.fTopologyTableItemSet[topologyTableItemIndex].fTopologyTableTuple;
            topologyTuple->distance = fTopologyTableTuple.distance;
            // DEBUG_PRINT_ALGO_DATA("F TABLE ORIGINATORADDRESS: %u\n", topologyTuple->originatorAddress);
            // DEBUG_PRINT_ALGO_DATA("F TABLE DESTINATIONADDRESS: %u\n", topologyTuple->destinationAddress);
            // DEBUG_PRINT_ALGO_DATA("F TABLE DISTANCE: %u\n", topologyTuple->distance);
        }
    }
}