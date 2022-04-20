#include <string.h>

#include "floodingStruct.h"
#include "rangingProtocolDebug.h"

// flooding check table
void FCheckTableInit(fCheckTable_t *checkTable)
{
    DEBUG_PRINT_STRUCT("START CHECK TABLE INIT\n");
    index_t i;
    for (i = 0; i < F_CHECK_TABLE_ITEM_SET_MAX_SIZE -1; i++)
    {
        checkTable->fCheckTableItemSet[i].next = i + 1;
    }
    checkTable->fCheckTableItemSet[i].next = -1;
    checkTable->fCheckTableFreeEntry = 0;
    checkTable->fCheckTableFullEntry = -1;
    checkTable->fCheckTableSize = 0;
}

static index_t FCheckTableMalloc(fCheckTable_t *checkTable)
{
    // 检查表内没有剩余空间
    if(checkTable->fCheckTableFreeEntry == -1)
    {
        // return -1;
        // TODO: 没有剩余空间了就将队列最后一个元素剔除，并将新元素插入队头
        index_t candidate = checkTable->fCheckTableFullEntry;
        while (checkTable->fCheckTableItemSet[checkTable->fCheckTableItemSet[candidate].next].next != -1)
        {
            candidate = checkTable->fCheckTableItemSet[candidate].next;
        }
        index_t tmp = checkTable->fCheckTableFullEntry;
        checkTable->fCheckTableFullEntry = checkTable->fCheckTableItemSet[candidate].next;
        checkTable->fCheckTableItemSet[candidate].next = -1;
        checkTable->fCheckTableItemSet[checkTable->fCheckTableFullEntry].next = tmp;
        candidate = checkTable->fCheckTableFullEntry;
        return candidate;
    }
    // 检查表内有剩余空间，进行分配
    else
    {
        index_t candidate = checkTable->fCheckTableFreeEntry;
        checkTable->fCheckTableFreeEntry = checkTable->fCheckTableItemSet[candidate].next;
        index_t tmp = checkTable->fCheckTableFullEntry;
        checkTable->fCheckTableFullEntry = candidate;
        checkTable->fCheckTableItemSet[candidate].next = tmp;
        return candidate;
    }
}

index_t FCheckTableInsert(fCheckTable_t *checkTable, uint16_t originatorAddress, uint16_t sequence)
{
    DEBUG_PRINT_STRUCT("START CHECK TABLE INSERT\n");
    index_t candidate = FCheckTableMalloc(checkTable);
    if (candidate != -1)
    {
        checkTable->fCheckTableItemSet[candidate].originatorAddress = originatorAddress;
        checkTable->fCheckTableItemSet[candidate].sequence = sequence;
        checkTable->fCheckTableSize++;
    }
    return candidate;
}

index_t FFindInCheckTable(fCheckTable_t *checkTable, uint16_t originatorAddress)
{
    DEBUG_PRINT_STRUCT("START CHECK TABLE FIND\n");
    index_t candidate = checkTable->fCheckTableFullEntry;
    // 头情况:头部是目标，则直接返回
    if (checkTable->fCheckTableItemSet[candidate].originatorAddress == originatorAddress)
    {
        return candidate;
    }
    // 中间情况,包括了尾情况：检测出目标，则将目标排列到队列的最前面
    while (checkTable->fCheckTableItemSet[candidate].next != -1)
    {
        index_t index = checkTable->fCheckTableItemSet[candidate].next;
        if(fCheckTable.fCheckTableItemSet[index].originatorAddress == originatorAddress)
        {
            checkTable->fCheckTableItemSet[candidate].next = checkTable->fCheckTableItemSet[index].next;
            checkTable->fCheckTableItemSet[index].next = checkTable->fCheckTableFullEntry;
            checkTable->fCheckTableFullEntry = index;
            candidate = index;
            return candidate;
        }
        candidate = index;
    }
    // 没有检测出目标，则返回-1
    candidate = checkTable->fCheckTableItemSet[candidate].next;
    return candidate;
}

// Flooding topology table
void FTopologyTableInit(fTopologyTable_t *topologyTable)
{
    DEBUG_PRINT_STRUCT("START TOPOLOGY TABLE INIT\n");
    index_t i;
    for (i = 0; i < F_TOPOLOGY_TABLE_ITEM_SET_MAX_SIZE -1; i++)
    {
        topologyTable->fTopologyTableItemSet[i].next = i + 1;
    }
    topologyTable->fTopologyTableItemSet[i].next = -1;
    topologyTable->fTopologyTableFreeEntry = 0;
    topologyTable->fTopologyTableFullEntry = -1;
    topologyTable->fTopologyTableSize = 0;
}

static index_t FTopologyTableMalloc(fTopologyTable_t *topologyTable)
{
    // 拓扑表内没有剩余空间
    if(topologyTable->fTopologyTableFreeEntry == -1)
    {
        return -1;
    }
    // 拓扑表内有剩余空间，进行分配
    else
    {
        index_t candidate = topologyTable->fTopologyTableFreeEntry;
        topologyTable->fTopologyTableFreeEntry = topologyTable->fTopologyTableItemSet[candidate].next;
        index_t tmp = topologyTable->fTopologyTableFullEntry;
        topologyTable->fTopologyTableFullEntry = candidate;
        topologyTable->fTopologyTableItemSet[candidate].next = tmp;
        return candidate;
    }
}

index_t FTopologyTableInsert(fTopologyTable_t *topologyTable, fTopologyTableTuple_t *topologyTuple)
{
    DEBUG_PRINT_STRUCT("START TOPOLOGY TABLE INSERT\n");
    index_t candidate = FTopologyTableMalloc(topologyTable);
    // DEBUG_PRINT_STRUCT_DATA("CANDIDATE-1: %d\n", candidate);
    // 泛洪拓扑表有剩余空间
    if(candidate != -1)
    {
        memcpy(&topologyTable->fTopologyTableItemSet[candidate].fTopologyTableTuple, topologyTuple, sizeof(fTopologyTableTuple_t));
        topologyTable->fTopologyTableSize++;
    }
    // DEBUG_PRINT_STRUCT_DATA("CANDIDATE-2: %d\n", candidate);
    return candidate;
}

index_t FFindInTopologyTable(fTopologyTable_t *topologyTable, uint16_t originatorAddress, uint16_t destinationAddress)
{
    DEBUG_PRINT_STRUCT("START TOPOLOGY TABLE FIND\n");
    index_t candidate = topologyTable->fTopologyTableFullEntry;
    DEBUG_PRINT_STRUCT_DATA("CANDIDATE-1: %d\n", candidate);
    while (candidate != -1)
    {
        fTopologyTableItem_t item = topologyTable->fTopologyTableItemSet[candidate];
        if (item.fTopologyTableTuple.originatorAddress == originatorAddress && item.fTopologyTableTuple.destinationAddress == destinationAddress)
        {
            break;
        }
        candidate = item.next;
    }
    DEBUG_PRINT_STRUCT_DATA("CANDIDATE-2: %d\n", candidate);
    return candidate;
}

static void FPrintTopologyTableTuple(fTopologyTableTuple_t *tuple)
{
    // DEBUG_PRINT_STRUCT_DATA("ORIGINATORADDRESS: %u  ", tuple->originatorAddress);
    // DEBUG_PRINT_STRUCT_DATA("DESTINATIONADDRESS: %u  ", tuple->destinationAddress);
    // DEBUG_PRINT_STRUCT_DATA("SEQUENCE: %u  ", tuple->sequence);
    // DEBUG_PRINT_STRUCT_DATA("DISTANCE: %u\n", tuple->distance);
}

void FPrintTopologyTable(fTopologyTable_t *table)
{
    for (index_t index = table->fTopologyTableFullEntry; index != -1; index = table->fTopologyTableItemSet[index].next)
    {
        FPrintTopologyTableTuple(&table->fTopologyTableItemSet[index].fTopologyTableTuple);
    }
}