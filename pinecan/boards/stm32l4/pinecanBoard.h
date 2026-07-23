#pragma once

#include "stm32l4xx_hal.h"
#include "canard.h"
#include "pinecan.h"

#ifndef RX_QUEUE_SIZE 
#define RX_QUEUE_SIZE 32
#endif

typedef struct {
    CAN_HandleTypeDef *hcan;
    CanardInstance *canard;
    struct uavcan_protocol_NodeStatus *nodeStatus;
} PinecanInit;

/**
 * @brief  Initialize PineCAN.
 * @param  initParams pointer to PinecanInit structure with initialization parameters.
 * @retval None
 */
PineCAN_Status pinecanInit(PinecanInit *initParams);
