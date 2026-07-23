#pragma once

#include "pinecanCommon.h"

typedef struct {
    CanardInstance *canard;
    struct uavcan_protocol_NodeStatus *nodeStatus;
} CommonInitParams;

void init(CommonInitParams *initParams);
void handleRxFrame(CanardCANFrame *rxFrame);
bool enqueueRxQueue(const CanardCANFrame *frame);
CanardCANFrame* dequeueRxQueue();
void processCanardRxQueue();
void handleRxFrame(CanardCANFrame *rxFrame);
CanardCANFrame* peekRxQueue();
