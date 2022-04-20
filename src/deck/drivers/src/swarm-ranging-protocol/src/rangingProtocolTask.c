#define DEBUG_MODULE "TS"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "mac.h"
#include "log.h"

#include "rangingProtocolTask.h"
#include "rangingProtocolDebug.h"
#include "rangingProtocolPacket.h"
#include "floodingAlgo.h"
#include "routingAlgo.h"

#define SEND_QUEUE_LENGTH 15
#define RECV_QUEUE_LENGTH 15
#define ANTENNA_OFFSET 154.0 // In meter
// about device
extern tsAddress_t myAddress;
static dwDevice_t* dwDevice;
static uint16_t devVelocityXId;
static uint16_t devVelocityYId;
static uint16_t devVelocityZId;
static float velocity;
// about FreeRTOS struct
static QueueHandle_t globalSendQueue;
static QueueHandle_t globalRecvQueue;
static SemaphoreHandle_t globalSetLock;
// about global message
packet_t txPacketCache = {0};
tsPacketWithTimestamp_t rxPacketWTsCache = {0};
packet_t *rxPacketCache = &rxPacketWTsCache.packet;
// about Ts message
static uint16_t globalMessageSeq = 0;
packet_t txTsPacket = {0};
// about F message
#ifdef CONFIG_SWARM_FLOODING
packet_t txFPacket = {0};
bool isFForwarding = false;
#endif
// about R Message
packet_t txRPacket = {0};

const int antennaDelay = (ANTENNA_OFFSET * 499.2e6 * 128) / 299792458.0; // In radio tick

// struct init
static void MessageQueueInit()
{
    globalSendQueue = xQueueCreate(SEND_QUEUE_LENGTH, sizeof(packet_t));
    globalRecvQueue = xQueueCreate(RECV_QUEUE_LENGTH, sizeof(tsPacketWithTimestamp_t));
}

static void SemaphoreInit()
{
    globalSetLock = xSemaphoreCreateMutex();
}

void TaskStructInit(dwDevice_t *dev)
{
    dwDevice = dev;

    MessageQueueInit();
    SemaphoreInit(dev);
}

// message methods
static uint16_t getMessageSeq()
{
  return globalMessageSeq++;
}

// call back
void TxCallback(dwDevice_t *dev)
{
    if (txPacketCache.fcf_s.type != MAC802154_TYPE_TS) return;
    // DEBUG_PRINT_TASK("TX CALLBACK SUCCESS\n");

    message_t *message = (message_t *) txPacketCache.payload;
    tsMessage_t *tsMessage = (tsMessage_t *) message->messagePayload;
    //DEBUG_PRINT_TASK_DATA("LAST TX MESSAGESEQENCE IS: %u\n", tsMessage->tsMessageHeader.lastTxSequence);
    //DEBUG_PRINT_TASK_DATA("TX MESSAGESEQENCE IS: %u\n", tsMessage->tsMessageHeader.messageSequence);

    dwTime_t txTimestamp = {.full = 0};
    dwGetTransmitTimestamp(dev, &txTimestamp);
    txTimestamp.full += (antennaDelay / 2);
    UpdateTxTs(txTimestamp, tsMessage->tsMessageHeader.messageSequence);
}

void RxCallback(dwDevice_t *dev) 
{
    unsigned int dataLength = dwGetDataLength(dwDevice);
    //DEBUG_PRINT_TASK_DATA("RX CALL BACK LENGTH: %u\n", dataLength);
    if (dataLength == 0) 
    {
        return;
    }
    DEBUG_PRINT_TASK("RX CALLBACK SUCCESS\n");

    memset(rxPacketCache, 0, sizeof(packet_t));
    dwGetData(dwDevice, (uint8_t *) rxPacketCache, dataLength);

    if(rxPacketCache->fcf_s.type == MAC802154_TYPE_TS)
    {
        dwTime_t *arriveTimestamp = &rxPacketWTsCache.rxTimestamp.timestamp;
        dwGetReceiveTimestamp(dev, arriveTimestamp);
        arriveTimestamp->full -= (antennaDelay / 2);

        message_t *message = (message_t *) &rxPacketCache->payload;
        tsMessageHeader_t *tsMessageHeader = (tsMessageHeader_t *) message->messagePayload;
        rxPacketWTsCache.rxTimestamp.sequenceNumber = tsMessageHeader->messageSequence;
    }

    xQueueSend(globalRecvQueue, &rxPacketWTsCache, portMAX_DELAY);
}

// TS task
static tsTime_t SendTs()
{
    // DEBUG_PRINT_TASK("START SEND TS\n");

    tsTime_t tsNextSendTime = xTaskGetTickCount() + M2T(TS_INTERVAL_MAX) + M2T(TS_INTERVAL_MIN);
    uint16_t tsMessageSeq = getMessageSeq();

    float velocityX = logGetFloat(devVelocityXId);
    float velocityY = logGetFloat(devVelocityYId);
    float velocityZ = logGetFloat(devVelocityZId);
    velocity = sqrt(pow(velocityX, 2) + pow(velocityY, 2) + pow(velocityZ, 2));
    
    GenerateTs(tsMessageSeq, velocity, myAddress, &tsNextSendTime, &txTsPacket);

    // DEBUG_PRINT_ALGO_DATA("TX MESSAGE SEQENCE: %u\n", tsMessageHeader->messageSequence);
    xQueueSend(globalSendQueue, &txTsPacket, portMAX_DELAY);
    // DEBUG_PRINT_ALGO_DATA("TX MESSAGE SEQENCE: %u\n", tsMessageHeader->messageSequence);
    return tsNextSendTime;
}

void TsTask(void *ptr)
{
    // get device's velocity log ID
    devVelocityXId = logGetVarId("stateEstimate", "vx");
    devVelocityYId = logGetVarId("stateEstimate", "vy");
    devVelocityZId = logGetVarId("stateEstimate", "vz");

    MAC80215_PACKET_INIT(txTsPacket, MAC802154_TYPE_TS);
    // task loop
    while (true) 
    {
        xSemaphoreTake(globalSetLock, portMAX_DELAY);
        // critical section
        tsTime_t currentTime = xTaskGetTickCount();
        tsTime_t nextSendTime = SendTs();
        if (nextSendTime < currentTime + M2T(TS_INTERVAL_MIN)) 
        {
            nextSendTime = currentTime + M2T(TS_INTERVAL_MIN);
        }
        //
        xSemaphoreGive(globalSetLock);
        vTaskDelay(nextSendTime - currentTime);
    }
}

// F task
#ifdef CONFIG_SWARM_FLOODING
// void FTask(void *ptr)
// {
//     MAC80215_PACKET_INIT(txFPacket, MAC802154_TYPE_F);
//     // task loop
//     while (true)
//     {
//         xSemaphoreTake(globalSetLock, portMAX_DELAY);
//         // critical section
//         GenerateF(myAddress, &txFPacket, 3);
//         DEBUG_PRINT_TASK("START SEND F\n");
//         xQueueSend(globalSendQueue, &txFPacket, portMAX_DELAY);
//         //
//         xSemaphoreGive(globalSetLock);
//         vTaskDelay(M2T(F_INTERVAL));
//     }
// }

// R task
void RTask(void *ptr)
{
    MAC80215_PACKET_INIT(txRPacket, MAC802154_TYPE_R);
    // task loop
    while (true)
    {
        xSemaphoreTake(globalSetLock, portMAX_DELAY);
        // critical section
        Routing();
        uint16_t destAddress = rand() % 3 + 1;
        if (destAddress == myAddress)
        {
            destAddress = (destAddress + 1) % 3;
            if (destAddress == 0)
            {
                destAddress = 3;
            }
        }
        if(GenerateR(&txRPacket, destAddress, 3))
        {
            xQueueSend(globalSendQueue, &txRPacket, portMAX_DELAY);
        }
        //
        xSemaphoreGive(globalSetLock);
        vTaskDelay(M2T(R_INTERVAL));
    }
}
#endif

// send task
void SendTask(void *ptr)
{
    TickType_t timeToWaitForSendQueue;
    timeToWaitForSendQueue = portMAX_DELAY;
    dwDevice = (dwDevice_t *) ptr;
    // MAC80215_PACKET_INIT(txPacketCache, MAC802154_TYPE_TS);
    // task loop
    while(true)
    {    
        if(xQueueReceive(globalSendQueue, &txPacketCache, timeToWaitForSendQueue))
        {
            message_t *txPacketCacheMessage = (message_t *) txPacketCache.payload;
            uint16_t messageLength = txPacketCacheMessage->messageHeader.messageLength;

            dwNewTransmit(dwDevice);
            dwSetDefaults(dwDevice);
            dwWaitForResponse(dwDevice, true);
            dwReceivePermanently(dwDevice, true);
            dwSetData(dwDevice, (uint8_t *) &txPacketCache, MAC802154_HEADER_LENGTH + messageLength);
            dwStartTransmit(dwDevice);
            DEBUG_PRINT_TASK("SEND SUCCESS\n");
        }   
        vTaskDelay(70);
    }
}

// receive task
static void PacketDispatch(tsPacketWithTimestamp_t *rxPacketWTs)
{
    DEBUG_PRINT_TASK("ENTER DISPATCH\n");
    tsTimestampTuple_t *rxTimestamp = &rxPacketWTs->rxTimestamp;
    packet_t *packet = (packet_t *) &rxPacketWTs->packet;
    message_t *message = (message_t *) &packet->payload;

    xSemaphoreTake(globalSetLock, portMAX_DELAY);

    uint16_t type = packet->fcf_s.type;
    
    switch (type)
    {
    case MAC802154_TYPE_TS:
        UpdateRxTs(message, rxTimestamp, myAddress);
        break;
    
    #ifdef CONFIG_SWARM_FLOODING
    case MAC802154_TYPE_F:
        DEBUG_PRINT_TASK("F MESSAGE HANDLE\n");
        // 检验泛洪信息是否重复，允许转发则转发
        if(CheckRxF(message, myAddress))
        {
            UpdateRxF(message);
            xQueueSend(globalSendQueue, packet, portMAX_DELAY);
        }
        break;

    case MAC802154_TYPE_R:
        if (DispatchR(message))
        {
            xQueueSend(globalSendQueue, packet, portMAX_DELAY);
        }
        break;
        
    #endif

    
    default:
        break;
    }

    xSemaphoreGive(globalSetLock);
}

void RecvTask(void *ptr)
{
    while(xQueueReceive(globalRecvQueue, &rxPacketWTsCache, portMAX_DELAY))
    {
        DEBUG_PRINT_TASK("RECV SUCCESS\n");
        PacketDispatch(&rxPacketWTsCache);
    }
}