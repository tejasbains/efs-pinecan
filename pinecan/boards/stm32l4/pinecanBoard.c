#include <string.h>
#include <stdio.h>
#include "pinecanBoard.h"
#include "pinecanCommon_internals.h"

/* ============ PRIVATE DATA ============ */
typedef struct {
    CAN_HandleTypeDef *hcan;
    uint8_t hardwareID[16];
} PinecanBoardData;

static PinecanBoardData boardData;

/* ============ PRIVATE FUNCTION DECLARATIONS ============ */

static void setUniqueHardwareID(uint8_t id[16]);

/* ============ PRIVATE FUNCTION DEFINITIONS ============ */

// get a 16 byte unique ID for this node, this should be based on the CPU unique ID or other unique ID
static void setUniqueHardwareID(uint8_t id[16]){
    uint32_t HALUniqueIDs[4];
    // Create Unique ID out of the 96-bit STM32 UID
    HALUniqueIDs[0] = HAL_GetUIDw0();
    HALUniqueIDs[1] = HAL_GetUIDw1();
    HALUniqueIDs[2] = HAL_GetUIDw2();
    HALUniqueIDs[3] = HAL_GetUIDw0(); // repeating UIDw0 for remaining bytes
    memcpy(id, HALUniqueIDs, 16);
}

/* ============ INTERNAL PUBLIC FUNCTION DEFINITIONS ============ */

uint8_t *getUniqueHardwareID(void) {
    return boardData.hardwareID;
}

uint32_t getUptimeMs(void) {
    return HAL_GetTick();
}

bool processTxMessage(const CanardCANFrame *txFrame) {
    if (HAL_CAN_GetTxMailboxesFreeLevel(boardData.hcan) > 0) {
        CAN_TxHeaderTypeDef txHeader = {0};

        if (txFrame->id & CANARD_CAN_FRAME_EFF) {
            txHeader.IDE = CAN_ID_EXT;
            txHeader.ExtId = txFrame->id & CANARD_CAN_EXT_ID_MASK;
        } else {
            txHeader.IDE = CAN_ID_STD;
            txHeader.StdId = txFrame->id & CANARD_CAN_STD_ID_MASK;
        }

        txHeader.DLC = txFrame->data_len;

        if (txFrame->id & CANARD_CAN_FRAME_RTR) {
            txHeader.RTR = CAN_RTR_REMOTE;
        } else {
            txHeader.RTR = CAN_RTR_DATA;
        }

        txHeader.TransmitGlobalTime = DISABLE;

        uint32_t txMailboxUsed = 0;
        if(HAL_CAN_AddTxMessage(boardData.hcan, &txHeader, txFrame->data, &txMailboxUsed) == HAL_OK)
        {
            return true;
        } else {
            // Failed to add message
            return false;
        }
    } else {
        // No free mailbox
        return false;
    }
}

/* ============ EXTERNAL PUBLIC FUNCTION DEFINITIONS ============ */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader = {0};
    uint8_t rxData[8] = {0};
    CanardCANFrame rxFrame = {0};

    // receive HAL CAN packet
    if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
    {
        // Process ID to canard format
        rxFrame.id = rxHeader.ExtId;

        if (rxHeader.IDE == CAN_ID_EXT) { // canard will only process the message if it is extended ID
            rxFrame.id |= CANARD_CAN_FRAME_EFF;
        }

        if (rxHeader.RTR == CAN_RTR_REMOTE) { // canard won't process the message if it is a remote frame
            rxFrame.id |= CANARD_CAN_FRAME_RTR;
        }

        rxFrame.data_len = rxHeader.DLC;
        memcpy(rxFrame.data, rxData, rxHeader.DLC);

        // assume a single interface
        rxFrame.iface_id = 0;
        
        // add frame from hardware queue to software queue
        enqueueRxQueue(&rxFrame);
    }
}

PineCAN_Status pinecanInit(PinecanInit *initParams) {
    boardData.hcan = initParams->hcan;

    // configure HAL CAN
    CAN_FilterTypeDef canfil;
    canfil.FilterBank = 0;
    canfil.FilterMode = CAN_FILTERMODE_IDMASK;
    canfil.FilterFIFOAssignment = CAN_RX_FIFO0;
    canfil.FilterIdHigh = 0;
    canfil.FilterIdLow = 0;
    canfil.FilterMaskIdHigh = 0;
    canfil.FilterMaskIdLow = 0;
    canfil.FilterScale = CAN_FILTERSCALE_32BIT;
    canfil.FilterActivation = ENABLE;
    canfil.SlaveStartFilterBank = 0;
    if (HAL_CAN_ConfigFilter(initParams->hcan, &canfil) != HAL_OK) {
        return PINECAN_ERROR;
    }

    if (HAL_CAN_ActivateNotification(initParams->hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return PINECAN_ERROR;
    }

    // start HAL driver
    if (HAL_CAN_Start(initParams->hcan) != HAL_OK) {
        return PINECAN_ERROR;
    }

    // set hardware id
    setUniqueHardwareID(boardData.hardwareID);

    // initialize non board specific
    CommonInitParams commonInitParams = {
        .canard = initParams->canard,
        .nodeStatus = initParams->nodeStatus
    };
    init(&commonInitParams);

    return PINECAN_OK;
}
