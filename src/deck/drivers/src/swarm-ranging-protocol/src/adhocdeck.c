#define DEBUG_MODULE "DWM"

#include <stdint.h>
#include <string.h>
#include "stm32fxxx.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "deck.h"
#include "system.h"
#include "debug.h"
#include "log.h"
#include "param.h"
#include "nvicconf.h"
#include "estimator.h"
#include "statsCnt.h"

#include "adhocdeck.h"
#include "lpsTdoa2Tag.h"
#include "lpsTdoa3Tag.h"
#include "lpsTwrTag.h"
#include "rangingProtocolInit.h"
#include "rangingProtocolTask.h"
//#include "uwbOlsr.h"
//#include "olsrStruct.h"
//#include "olsrAlgo.h"

#define CS_PIN DECK_GPIO_IO1

// LOCO deck alternative IRQ and RESET pins(IO_2, IO_3) instead of default (RX1, TX1), leaving UART1 free for use
#ifdef LOCODECK_USE_ALT_PINS
#define GPIO_PIN_IRQ 	  DECK_GPIO_IO2
#define GPIO_PIN_RESET 	DECK_GPIO_IO3
#define EXTI_PortSource EXTI_PortSourceGPIOB
#define EXTI_PinSource 	EXTI_PinSource5
#define EXTI_LineN 		  EXTI_Line5
#define EXTI_IRQChannel EXTI9_5_IRQn
#else
#define GPIO_PIN_IRQ      DECK_GPIO_RX1
#define GPIO_PIN_RESET    DECK_GPIO_TX1
#define EXTI_PortSource EXTI_PortSourceGPIOC
#define EXTI_PinSource    EXTI_PinSource11
#define EXTI_LineN          EXTI_Line11
#define EXTI_IRQChannel EXTI15_10_IRQn
#endif

#define DEFAULT_RX_TIMEOUT 0

#define ANTENNA_OFFSET 154.6   // In meter

// The anchor position can be set using parameters
// As an option you can set a static position in this file and set
// combinedAnchorPositionOk to enable sending the anchor rangings to the Kalman filter

static lpsAlgoOptions_t algoOptions = {
    // .userRequestedMode is the wanted algorithm, available as a parameter
#if LPS_TDOA_ENABLE
    .userRequestedMode = lpsMode_TDoA2,
#elif LPS_TDOA3_ENABLE
    .userRequestedMode = lpsMode_TDoA3,
#elif defined(LPS_TWR_ENABLE)
    .userRequestedMode = lpsMode_TWR,
#else
    .userRequestedMode = lpsMode_auto,
#endif
    // .currentRangingMode is the currently running algorithm, available as a log
    // lpsMode_auto is an impossible mode which forces initialization of the requested mode
    // at startup
    .currentRangingMode = lpsMode_auto,
    .modeAutoSearchActive = true,
    .modeAutoSearchDoInitialize = true,
};

struct {
  uwbAlgorithm_t *algorithm;
  char *name;
} algorithmsList[LPS_NUMBER_OF_ALGORITHMS + 1] = {
    [lpsMode_TWR] = {.algorithm = &uwbTwrTagAlgorithm, .name="TWR"},
    [lpsMode_TDoA2] = {.algorithm = &uwbTdoa2TagAlgorithm, .name="TDoA2"},
    [lpsMode_TDoA3] = {.algorithm = &uwbTdoa3TagAlgorithm, .name="TDoA3"},
};

#if LPS_TDOA_ENABLE
static uwbAlgorithm_t *algorithm = &uwbTdoa2TagAlgorithm;
#elif LPS_TDOA3_ENABLE
static uwbAlgorithm_t *algorithm = &uwbTdoa3TagAlgorithm;
#else
static uwbAlgorithm_t *algorithm = &uwbTwrTagAlgorithm;
#endif

static bool isInit = false;
static TaskHandle_t uwbTaskHandle = 0;
static SemaphoreHandle_t algoSemaphore;
static dwDevice_t dwm_device;
static dwDevice_t *dwm = &dwm_device;

static QueueHandle_t lppShortQueue;

static STATS_CNT_RATE_DEFINE(spiWriteCount, 1000);
static STATS_CNT_RATE_DEFINE(spiReadCount, 1000);

static void txCallback(dwDevice_t *dev) {
  //olsrTxCallback(dev);
  TxCallback(dev);
  return;
}

static void rxCallback(dwDevice_t *dev) {
  // olsrRxCallback(dev);
  // olsrPacketLossCallBack(dev);
  RxCallback(dev);
}

static void rxTimeoutCallback(dwDevice_t *dev) {
  return;
}

// This function is called from the memory sub system that runs in a different
// task, protect it from concurrent calls from this task
// TODO krri Break the dependency, do not call directly from other modules into the deck driver
bool locoDeckGetAnchorPosition(const uint8_t anchorId, point_t *position) {
  if (!isInit) {
    return false;
  }

  xSemaphoreTake(algoSemaphore, portMAX_DELAY);
  bool result = algorithm->getAnchorPosition(anchorId, position);
  xSemaphoreGive(algoSemaphore);
  return result;
}

// This function is called from the memory sub system that runs in a different
// task, protect it from concurrent calls from this task
// TODO krri Break the dependency, do not call directly from other modules into the deck driver
uint8_t locoDeckGetAnchorIdList(uint8_t unorderedAnchorList[], const int maxListSize) {
  if (!isInit) {
    return 0;
  }

  xSemaphoreTake(algoSemaphore, portMAX_DELAY);
  uint8_t result = algorithm->getAnchorIdList(unorderedAnchorList, maxListSize);
  xSemaphoreGive(algoSemaphore);
  return result;
}

// This function is called from the memory sub system that runs in a different
// task, protect it from concurrent calls from this task
// TODO krri Break the dependency, do not call directly from other modules into the deck driver
uint8_t locoDeckGetActiveAnchorIdList(uint8_t unorderedAnchorList[], const int maxListSize) {
  if (!isInit) {
    return 0;
  }
  xSemaphoreTake(algoSemaphore, portMAX_DELAY);
  uint8_t result = algorithm->getActiveAnchorIdList(unorderedAnchorList, maxListSize);
  xSemaphoreGive(algoSemaphore);
  return result;
}

// static bool switchToMode(const lpsMode_t newMode) {
//   bool result = false;

//   if (lpsMode_auto != newMode && newMode <= LPS_NUMBER_OF_ALGORITHMS) {
//     algoOptions.currentRangingMode = newMode;
//     algorithm = algorithmsList[algoOptions.currentRangingMode].algorithm;

//     algorithm->init(dwm);
//     timeout = algorithm->onEvent(dwm, eventTimeout);

//     result = true;
//   }

//   return result;
// }

// static void autoModeSearchTryMode(const lpsMode_t newMode, const uint32_t now) {
//   // Set up next time to check
//   algoOptions.nextSwitchTick = now + LPS_AUTO_MODE_SWITCH_PERIOD;
//   switchToMode(newMode);
// }

// static lpsMode_t autoModeSearchGetNextMode() {
//   lpsMode_t newMode = algoOptions.currentRangingMode + 1;
//   if (newMode > LPS_NUMBER_OF_ALGORITHMS) {
//     newMode = lpsMode_TWR;
//   }

//   return newMode;
// }

// static void processAutoModeSwitching() {
//   uint32_t now = xTaskGetTickCount();

//   if (algoOptions.modeAutoSearchActive) {
//     if (algoOptions.modeAutoSearchDoInitialize) {
//       autoModeSearchTryMode(lpsMode_TDoA2, now);
//       algoOptions.modeAutoSearchDoInitialize = false;
//     } else {
//       if (now > algoOptions.nextSwitchTick) {
//         if (algorithm->isRangingOk()) {
//           // We have found an algorithm, stop searching and lock to it.
//           algoOptions.modeAutoSearchActive = false;
//           DEBUG_PRINT("Automatic mode: detected %s\n", algorithmsList[algoOptions.currentRangingMode].name);
//         } else {
//           lpsMode_t newMode = autoModeSearchGetNextMode();
//           autoModeSearchTryMode(newMode, now);
//         }
//       }
//     }
//   }
// }

// static void resetAutoSearchMode() {
//   algoOptions.modeAutoSearchActive = true;
//   algoOptions.modeAutoSearchDoInitialize = true;
// }

// static void handleModeSwitch() {
//   if (algoOptions.userRequestedMode == lpsMode_auto) {
//     processAutoModeSwitching();
//   } else {
//     resetAutoSearchMode();
//     if (algoOptions.userRequestedMode != algoOptions.currentRangingMode) {
//       if (switchToMode(algoOptions.userRequestedMode)) {
//         DEBUG_PRINT("Switching to mode %s\n", algorithmsList[algoOptions.currentRangingMode].name);
//       }
//     }
//   }
// }

static void uwbTask(void *parameters) {
  // lppShortQueue = xQueueCreate(10, sizeof(lpsLppShortPacket_t));

  // algoOptions.currentRangingMode = lpsMode_auto;
  algorithm = &uwbAlgorithm;
  systemWaitStart();
  algorithm->init(dwm);
  while (1) {
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
      do {
        dwHandleInterrupt(dwm);
      } while (digitalRead(GPIO_PIN_IRQ) != 0);
    }
    // dwHandleInterrupt(dwm);
    // vTaskDelay(100);
    // xSemaphoreTake(algoSemaphore, portMAX_DELAY);
    // handleModeSwitch();
    // xSemaphoreGive(algoSemaphore);

    // if (ulTaskNotifyTake(pdTRUE, timeout / portTICK_PERIOD_MS) > 0) {
    //   do{
    //     xSemaphoreTake(algoSemaphore, portMAX_DELAY);
    //     dwHandleInterrupt(dwm);
    //     xSemaphoreGive(algoSemaphore);
    //   } while(digitalRead(GPIO_PIN_IRQ) != 0);
    // } else {
    //   xSemaphoreTake(algoSemaphore, portMAX_DELAY);
    //   timeout = algorithm->onEvent(dwm, eventTimeout);
    //   xSemaphoreGive(algoSemaphore);
    // }
  }
}

static lpsLppShortPacket_t lppShortPacket;

bool lpsSendLppShort(uint8_t destId, void *data, size_t length) {
  bool result = false;

  if (isInit) {
    lppShortPacket.dest = destId;
    lppShortPacket.length = length;
    memcpy(lppShortPacket.data, data, length);
    result = xQueueSend(lppShortQueue, &lppShortPacket, 0) == pdPASS;
  }

  return result;
}

bool lpsGetLppShort(lpsLppShortPacket_t *shortPacket) {
  return xQueueReceive(lppShortQueue, shortPacket, 0) == pdPASS;
}

static uint8_t spiTxBuffer[196];
static uint8_t spiRxBuffer[196];
static uint16_t spiSpeed = SPI_BAUDRATE_2MHZ;

/************ Low level ops for libdw **********/
static void spiWrite(dwDevice_t *dev, const void *header, size_t headerLength,
                     const void *data, size_t dataLength) {
  spiBeginTransaction(spiSpeed);
  digitalWrite(CS_PIN, LOW);
  memcpy(spiTxBuffer, header, headerLength);
  memcpy(spiTxBuffer + headerLength, data, dataLength);
  spiExchange(headerLength + dataLength, spiTxBuffer, spiRxBuffer);
  digitalWrite(CS_PIN, HIGH);
  spiEndTransaction();
  STATS_CNT_RATE_EVENT(&spiWriteCount);
}

static void spiRead(dwDevice_t *dev, const void *header, size_t headerLength,
                    void *data, size_t dataLength) {
  spiBeginTransaction(spiSpeed);
  digitalWrite(CS_PIN, LOW);
  memcpy(spiTxBuffer, header, headerLength);
  memset(spiTxBuffer + headerLength, 0, dataLength);
  spiExchange(headerLength + dataLength, spiTxBuffer, spiRxBuffer);
  memcpy(data, spiRxBuffer + headerLength, dataLength);
  digitalWrite(CS_PIN, HIGH);
  spiEndTransaction();
  STATS_CNT_RATE_EVENT(&spiReadCount);
}

#if LOCODECK_USE_ALT_PINS
void __attribute__((used)) EXTI5_Callback(void)
#else
void __attribute__((used)) EXTI11_Callback(void)
#endif
{
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

  NVIC_ClearPendingIRQ(EXTI_IRQChannel);
  EXTI_ClearITPendingBit(EXTI_LineN);

  // Unlock interrupt handling task
  vTaskNotifyGiveFromISR(uwbTaskHandle, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) portYIELD();
}

static void spiSetSpeed(dwDevice_t *dev, dwSpiSpeed_t speed) {
  if (speed == dwSpiSpeedLow) {
    spiSpeed = SPI_BAUDRATE_2MHZ;
  } else if (speed == dwSpiSpeedHigh) {
    spiSpeed = SPI_BAUDRATE_21MHZ;
  }
}

static void delayms(dwDevice_t *dev, unsigned int delay) {
  vTaskDelay(M2T(delay));
}

static dwOps_t dwOps = {
    .spiRead = spiRead,
    .spiWrite = spiWrite,
    .spiSetSpeed = spiSetSpeed,
    .delayms = delayms,
};

/*********** Deck driver initialization ***************/

static void dwm1000Init(DeckInfo *info) {
  EXTI_InitTypeDef EXTI_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  spiBegin();

  // Set up interrupt
  SYSCFG_EXTILineConfig(EXTI_PortSource, EXTI_PinSource);

  EXTI_InitStructure.EXTI_Line = EXTI_LineN;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  // Init pins
  pinMode(CS_PIN, OUTPUT);
  pinMode(GPIO_PIN_RESET, OUTPUT);
  pinMode(GPIO_PIN_IRQ, INPUT);

  // Reset the DW1000 chip
  digitalWrite(GPIO_PIN_RESET, 0);
  vTaskDelay(M2T(10));
  digitalWrite(GPIO_PIN_RESET, 1);
  vTaskDelay(M2T(10));

  // Initialize the driver
  dwInit(dwm, &dwOps);       // Init libdw

  int result = dwConfigure(dwm);
  if (result != 0) {
    isInit = false;
    DEBUG_PRINT("Failed to configure DW1000!\r\n");
    return;
  }

  dwEnableAllLeds(dwm);

  dwTime_t delay = {.full = 0};
  dwSetAntenaDelay(dwm, delay);

  dwAttachSentHandler(dwm, txCallback);
  dwAttachReceivedHandler(dwm, rxCallback);
  dwAttachReceiveTimeoutHandler(dwm, rxTimeoutCallback);

  dwNewConfiguration(dwm);
  dwSetDefaults(dwm);

#ifdef LPS_LONGER_RANGE
  dwEnableMode(dwm, MODE_SHORTDATA_MID_ACCURACY);
#else
  dwEnableMode(dwm, MODE_SHORTDATA_FAST_ACCURACY);
#endif

  dwSetChannel(dwm, CHANNEL_2);
  dwSetPreambleCode(dwm, PREAMBLE_CODE_64MHZ_9);

#ifdef LPS_FULL_TX_POWER
  dwUseSmartPower(dwm, false);
  dwSetTxPower(dwm, 0x1F1F1F1Ful);
#else
  dwUseSmartPower(dwm, true);
#endif

  dwSetReceiveWaitTimeout(dwm, 0);

  dwCommitConfiguration(dwm);

  // Enable interrupt
  NVIC_InitStructure.NVIC_IRQChannel = EXTI_IRQChannel;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_VERY_HIGH_PRI;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  algoSemaphore = xSemaphoreCreateMutex();

  xTaskCreate(uwbTask, LPS_DECK_TASK_NAME, 5 * configMINIMAL_STACK_SIZE, NULL,
              LPS_DECK_TASK_PRI, &uwbTaskHandle);

  isInit = true;
}

uint16_t locoDeckGetRangingState() {
  return algoOptions.rangingState;
}

void locoDeckSetRangingState(const uint16_t newState) {
  algoOptions.rangingState = newState;
}

static bool dwm1000Test() {
  if (!isInit) {
    DEBUG_PRINT("Error while initializing DWM1000\n");
  }

  return isInit;
}

static const DeckDriver dwm1000_deck = {
    .vid = 0xBC,
    .pid = 0x06,
    .name = "bcDWM1000",

    .usedGpio = 0,  // FIXME: set the used pins
    .requiredEstimator = kalmanEstimator,
#ifdef LOCODECK_NO_LOW_INTERFERENCE
    .requiredLowInterferenceRadioMode = false,
#else
    .requiredLowInterferenceRadioMode = true,
#endif

    .init = dwm1000Init,
    .test = dwm1000Test,
};

DECK_DRIVER(dwm1000_deck);

PARAM_GROUP_START(deck)
        PARAM_ADD(PARAM_UINT8 | PARAM_RONLY, bcDWM1000, &isInit)
PARAM_GROUP_STOP(deck)

LOG_GROUP_START(ranging)
        LOG_ADD(LOG_UINT16, state, &algoOptions.rangingState)
LOG_GROUP_STOP(ranging)

LOG_GROUP_START(loco)
        LOG_ADD(LOG_UINT8, mode, &algoOptions.currentRangingMode)
        STATS_CNT_RATE_LOG_ADD(spiWr, &spiWriteCount)
        STATS_CNT_RATE_LOG_ADD(spiRe, &spiReadCount)
LOG_GROUP_STOP(loco)

PARAM_GROUP_START(loco)
        PARAM_ADD(PARAM_UINT8, mode, &algoOptions.userRequestedMode)
PARAM_GROUP_STOP(loco)
