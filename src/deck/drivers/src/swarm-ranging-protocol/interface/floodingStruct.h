#ifndef __FLOODINGSTRUCT_H__
#define __FLOODINGSTRUCT_H__

#include <stdint.h>

#define F_CHECK_TABLE_ITEM_SET_MAX_SIZE 10
#define F_TOPOLOGY_TABLE_ITEM_SET_MAX_SIZE 20

typedef short index_t;

//Flooding check Table
typedef struct
{
    uint16_t originatorAddress;
    uint16_t sequence;
    index_t next;
} __attribute__((packed)) fCheckTableItem_t;

typedef struct
{
    fCheckTableItem_t fCheckTableItemSet[F_CHECK_TABLE_ITEM_SET_MAX_SIZE];
    index_t fCheckTableFreeEntry;
    index_t fCheckTableFullEntry;
    uint8_t fCheckTableSize;
} __attribute__((packed)) fCheckTable_t;

fCheckTable_t fCheckTable;

// Flooding topology Table
typedef struct {
    uint16_t originatorAddress;
    uint16_t destinationAddress;
    uint16_t distance;
} __attribute__((packed)) fTopologyTableTuple_t;

typedef struct {
    fTopologyTableTuple_t fTopologyTableTuple;
    index_t next;
} __attribute__((packed)) fTopologyTableItem_t; 

typedef struct {
    fTopologyTableItem_t fTopologyTableItemSet[F_TOPOLOGY_TABLE_ITEM_SET_MAX_SIZE];
    index_t fTopologyTableFreeEntry;
    index_t fTopologyTableFullEntry;
    uint8_t fTopologyTableSize;
} __attribute__((packed)) fTopologyTable_t;

fTopologyTable_t fTopologyTable;

// Flooding check table methods
void FCheckTableInit(fCheckTable_t *checkTable);
index_t FCheckTableInsert(fCheckTable_t *checkTable, uint16_t originatorAddress, uint16_t sequence);
index_t FFindInCheckTable(fCheckTable_t *checkTable, uint16_t originatorAddress);

// Flooding topology table methods
void FTopologyTableInit(fTopologyTable_t *topologyTable);
index_t FTopologyTableInsert(fTopologyTable_t *topologyTable, fTopologyTableTuple_t *topologyTuple);
index_t FFindInTopologyTable(fTopologyTable_t *topologyTable, uint16_t originatorAddress, uint16_t destinationAddress);
void FPrintTopologyTable(fTopologyTable_t *table);

#endif