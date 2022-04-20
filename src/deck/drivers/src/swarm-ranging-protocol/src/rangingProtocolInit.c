#define DEBUG_MODULE "TS"

#include "FreeRTOS.h"
#include "task.h"
#include "system.h"
#include "configblock.h"

#include "rangingProtocolInit.h"
#include "rangingProtocolDebug.h"
#include "rangingProtocolStruct.h"
#include "rangingProtocolTask.h"
#include "floodingStruct.h"

point_t anchorPosition[2];

static void TsTaskInit()
{
    if(xTaskCreate(TsTask, "TS_TASK", MINIMAL_STACK_SIZE, NULL, LPS_DECK_TASK_PRI, NULL) == pdPASS)
    {
        DEBUG_PRINT_INIT("TS TASK INIT SUCCESS\n");
    }
    else
    {
        DEBUG_PRINT_INIT("TS TASK INIT FAILED\n");
    }
}

#ifdef CONFIG_SWARM_FLOODING
static void FTaskInit()
{
    if(xTaskCreate(FTask, "F_TASK", MINIMAL_STACK_SIZE, NULL, LPS_DECK_TASK_PRI, NULL) == pdPASS)
    {
        DEBUG_PRINT_INIT("F TASK INIT SUCCESS\n");
    }
    else
    {
        DEBUG_PRINT_INIT("F TASK INIT FAILED\n");
    }
}
#endif

static void SendTaskInit(dwDevice_t *dev)
{
    if(xTaskCreate(SendTask, "SEND_TASK", 2 * MINIMAL_STACK_SIZE, dev, 3, NULL) == pdPASS)
    {
        DEBUG_PRINT_INIT("SEND TASK INIT SUCCESS\n");
    }
    else
    {
        DEBUG_PRINT_INIT("SEND TASK INIT FAILED\n");
    }
}

static void RecvTaskInit(dwDevice_t *dev)
{
    if(xTaskCreate(RecvTask, "RECV_TASK", 4 * MINIMAL_STACK_SIZE, dev, 4, NULL) == pdPASS)
    {
        DEBUG_PRINT_INIT("RECV TASK INIT SUCCESS\n");
    }
    else
    {
        DEBUG_PRINT_INIT("RECV TASK INIT FAILED\n");
    }
}

static void TaskInit(dwDevice_t *dev)
{
    DEBUG_PRINT_INIT("START TASK INIT\n");
    SendTaskInit(dev);
    RecvTaskInit(dev);

    TsTaskInit();
    #ifdef CONFIG_SWARM_FLOODING
    FTaskInit();
    #endif
}

static void DataInit()
{
    DEBUG_PRINT_INIT("START DATA INIT\n");
    int myChanel = configblockGetRadioChannel();
    myAddress = myChanel | 0x0000;
}

static void StructInit()
{
    DEBUG_PRINT_INIT("START STRUCT INIT\n");
    TsRangingTableInit(&tsRangingTable);
    #ifdef CONFIG_SWARM_FLOODING
    FCheckTableInit(&fCheckTable);
    FTopologyTableInit(&fTopologyTable);   
    #endif
}

static void Init(dwDevice_t *dev)
{
    DEBUG_PRINT_INIT("START INIT\n");
    systemWaitStart();

    DataInit();
    StructInit();
    TaskStructInit(dev);
    TaskInit(dev);
}

static bool IsRangingOk()
{
  return true;
}

static bool GetAnchorPosition(const uint8_t anchorId, point_t* position) {
    *position = anchorPosition[anchorId];
    return true;
}

static uint8_t GetAnchorIdList(uint8_t unorderedAnchorList[], const int maxListSize) {
  return 2;
}

static uint8_t GetActiveAnchorIdList(uint8_t unorderedAnchorList[], const int maxListSize) {
  return 2;
}

uwbAlgorithm_t uwbAlgorithm = {
    .init = Init,
    .isRangingOk = IsRangingOk,
    .getAnchorPosition = GetAnchorPosition,
    .getAnchorIdList = GetAnchorIdList,
    .getActiveAnchorIdList = GetActiveAnchorIdList,
};