#ifndef __RANGINGPROTOCOLTASK_H__
#define __RANGINGPROTOCOLTASK_H__

#include "FreeRTOS.h"
#include "semphr.h"

#include "adhocdeck.h"
#include "rangingProtocolAlgo.h"
#include "rangingProtocolStruct.h"

void TxCallback(dwDevice_t *dev);
void RxCallback(dwDevice_t *dev) ;
void TsTask(void *ptr);
void FTask(void *ptr);
void SendTask(void *ptr);
void RecvTask(void *ptr);
void TaskStructInit(dwDevice_t *dev);

#endif // __RANGINGPROTOCOLTASK_H__