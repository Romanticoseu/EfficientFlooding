#include "rangingProtocolStruct.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "rangingProtocolDebug.h"

// ranging table
void TsRangingTableInit(tsRangingTable_t *rangingTable) 
{
  // DEBUG_PRINT_STRUCT("START RANGING TABLE INIT\n");
  itemIndex_t i;
  for (i = 0; i < RANGING_TABLE_ITEM_SET_SIZE - 1; i++) 
  {
    rangingTable->itemSet[i].next = i + 1;
  }
  rangingTable->itemSet[i].next = -1;
  rangingTable->freeQueueEntry = 0;
  rangingTable->fullQueueEntry = -1;
  rangingTable->size = 0;
}

static bool TsRangingTableFree(tsRangingTable_t *rangingTable, itemIndex_t delItemIndex) 
{
  if (delItemIndex == -1) 
  {
    return true;
  }
  // DEBUG_PRINT_STRUCT("START RANGING TABLE FREE\n");
  //del from full queue
  itemIndex_t currentItemIndex = rangingTable->fullQueueEntry;
  if (delItemIndex == currentItemIndex) 
  {
    rangingTable->fullQueueEntry = rangingTable->itemSet[currentItemIndex].next;
    // insert to empty queue
    rangingTable->itemSet[delItemIndex].next = rangingTable->freeQueueEntry;
    rangingTable->freeQueueEntry = delItemIndex;
    rangingTable->size = rangingTable->size - 1;
    return true;
  } 
  else 
  {
    while (currentItemIndex != -1) 
    {
      if (rangingTable->itemSet[currentItemIndex].next == delItemIndex) 
      {
        rangingTable->itemSet[currentItemIndex].next = rangingTable->itemSet[delItemIndex].next;
        // insert to empty queue
        rangingTable->itemSet[delItemIndex].next = rangingTable->freeQueueEntry;
        rangingTable->freeQueueEntry = delItemIndex;
        rangingTable->size = rangingTable->size - 1;
        return true;
      }
      currentItemIndex = rangingTable->itemSet[currentItemIndex].next;
    }
  }
  return false;
}

bool TsRangingTableClearExpire(tsRangingTable_t *rangingTable) 
{
  // DEBUG_PRINT_STRUCT("START RANGING CLEAR EXPIRE\n");
  itemIndex_t candidate = rangingTable->fullQueueEntry;
  tsTime_t currentTime = xTaskGetTickCount();
  bool isChanged = false;
  while(candidate != -1)
  {
    tsRangingTableItem_t rangingTableItem = rangingTable->itemSet[candidate];
    if(rangingTableItem.rangingTuple.expiration < currentTime)
    {
      itemIndex_t nextItem = rangingTableItem.next;
      TsRangingTableFree(rangingTable, candidate);
      candidate = nextItem;
      isChanged = true;
      continue;
    }
    candidate = rangingTableItem.next;
  }
  return isChanged;
}

void TsSortRangingTable(tsRangingTable_t *rangingTable) 
{
  if (rangingTable->fullQueueEntry == -1) 
  {
    return;
  }
  // DEBUG_PRINT_STRUCT("START RANGING TABLE SORT\n");
  itemIndex_t headItemIndex = rangingTable->fullQueueEntry;
  itemIndex_t currentItemIndex = rangingTable->itemSet[headItemIndex].next;
  rangingTable->itemSet[headItemIndex].next = -1;
  itemIndex_t nextItemIndex = -1;
  while (currentItemIndex != -1) 
  {
    nextItemIndex = rangingTable->itemSet[currentItemIndex].next;
    if (rangingTable->itemSet[currentItemIndex].rangingTuple.nextDeliveryTime <= rangingTable->itemSet[headItemIndex].rangingTuple.nextDeliveryTime) 
    {
      rangingTable->itemSet[currentItemIndex].next = headItemIndex;
      headItemIndex = currentItemIndex;
    } 
    else
    {
      itemIndex_t start = rangingTable->itemSet[headItemIndex].next;
      itemIndex_t pre = headItemIndex;
      while (start != -1 && rangingTable->itemSet[currentItemIndex].rangingTuple.nextDeliveryTime
          > rangingTable->itemSet[start].rangingTuple.nextDeliveryTime) 
      {
        pre = start;
        start = rangingTable->itemSet[start].next;
      }
      rangingTable->itemSet[currentItemIndex].next = start;
      rangingTable->itemSet[pre].next = currentItemIndex;
    }
    currentItemIndex = nextItemIndex;
  }
  rangingTable->fullQueueEntry = headItemIndex;
}

void TsPrintRangingTableTuple(tsRangingTuple_t *tuple)
{

}

void TsPrintRangingTable(tsRangingTable_t *rangingTable) 
{
  for (itemIndex_t i = rangingTable->fullQueueEntry; i != -1; i = rangingTable->itemSet[i].next) 
  {
    TsPrintRangingTableTuple(&rangingTable->itemSet[i].rangingTuple);
  }
}

itemIndex_t TsFindInRangingTable(tsRangingTable_t *rangingTable, tsAddress_t address)
{
  // DEBUG_PRINT_STRUCT("START RANGING TABLE FIND\n");
  itemIndex_t index = rangingTable->fullQueueEntry;
  while (index != -1) 
  {
    tsRangingTableItem_t item = rangingTable->itemSet[index];
    if (item.rangingTuple.peerAddress == address) 
    {
      break;
    }
    index = item.next;
  }
  return index;
}

static itemIndex_t TsRangingTableMalloc(tsRangingTable_t *rangingTable) 
{
  if (rangingTable->freeQueueEntry == -1) 
  {
    return -1;
  }
  else 
  {
    // DEBUG_PRINT_STRUCT("START RANGING TABLE MALLOC\n");
    itemIndex_t candidate = rangingTable->freeQueueEntry;
    rangingTable->freeQueueEntry = rangingTable->itemSet[candidate].next;
    // insert to full queue
    itemIndex_t tmp = rangingTable->fullQueueEntry;
    rangingTable->fullQueueEntry = candidate;
    rangingTable->itemSet[candidate].next = tmp;
    return candidate;
  }
}

itemIndex_t TsRangingTableInsert(tsRangingTable_t *rangingTable, tsRangingTuple_t *tuple) 
{
  // DEBUG_PRINT_STRUCT("START RANGING TABLE INSERT\n");
  itemIndex_t candidate = TsRangingTableMalloc(rangingTable);
  if (candidate != -1) 
  {
    memcpy(&rangingTable->itemSet[candidate].rangingTuple, tuple, sizeof(tsRangingTuple_t));
    rangingTable->size++;
  }
  return candidate;
}

