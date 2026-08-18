#include "pinecanCommon.h"
#include "pinecanCommon_internals.h"
#include "pinecanBoard.h"
#include "pinecanBoard_internals.h"
#include "pinecan.h"
#include "dronecan_msgs.h"
#include "pinecan_handlers.h"
#include <string.h>

/* ============ PRIVATE DATA ============ */

typedef struct {
    CanardInstance *canard;
    struct uavcan_protocol_NodeStatus *nodeStatus;
} PinecanData;

typedef struct {
    uint8_t front;
    uint8_t back;
    uint8_t count;
    CanardCANFrame buffer[RX_QUEUE_SIZE];
} rxQueueData;

static rxQueueData rxQueue = {0};

static PinecanData data;

// libcanard stores types with 8-byte members in this pool.
static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE] __attribute__((aligned(8)));

/* ============ PRIVATE FUNCTION DECLARATIONS ============ */

static bool shouldAcceptTransfer(const CanardInstance* ins, uint64_t* crcSignature, uint16_t dataTypeID, CanardTransferType transferType, uint8_t sourceNodeID);
static void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer);
static void handleGetNodeInfo(CanardInstance* ins, CanardRxTransfer *transfer);
static PineCAN_Status sendNodeStatus(void);
static void processCanardTxQueue(void);

// Add any PineCAN specific rx handlers
#define RX_HANDLER_LIST_PRIVATE RX_HANDLER_LIST \
        REGISTER_RX_HANDLER(UAVCAN_PROTOCOL_GETNODEINFO,           handleGetNodeInfo,           REQUEST) \

// Compile-time signature check for all handlers
typedef void (*PinecanRxHandler)(CanardInstance*, CanardRxTransfer*);

__attribute__((unused)) static inline void _pinecan_check_handlers(void) {
    #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
        { PinecanRxHandler _h = (HANDLER); (void)_h; }
    RX_HANDLER_LIST_PRIVATE
    #undef REGISTER_RX_HANDLER
}

/* ============ PRIVATE FUNCTION DEFINITIONS ============ */

static bool shouldAcceptTransfer(const CanardInstance* ins, uint64_t* crcSignature, uint16_t dataTypeID, CanardTransferType transferType, uint8_t sourceNodeID) {
    PINECAN_UNUSED(ins);
    PINECAN_UNUSED(sourceNodeID);

    // --------- RESPONSE ----------
    if (transferType == CanardTransferTypeResponse)
    {
        switch (dataTypeID)
        {
            // For RESPONSE, we might have signature TYPE_RESPONSE_SIGNATURE (if you use it)
            #define DISPATCH_ACCEPT_CASE_RESPONSE(TYPE) \
                case TYPE##_ID: \
                    *crcSignature = TYPE##_RESPONSE_SIGNATURE; \
                    return true;

            #define DISPATCH_ACCEPT_CASE_REQUEST(TYPE)   /* nothing */
            #define DISPATCH_ACCEPT_CASE_BROADCAST(TYPE) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_ACCEPT_CASE_##KIND(TYPE)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_ACCEPT_CASE_RESPONSE
            #undef DISPATCH_ACCEPT_CASE_REQUEST
            #undef DISPATCH_ACCEPT_CASE_BROADCAST

            default:
                return false;
        }
    }

    // --------- REQUEST ----------
    if (transferType == CanardTransferTypeRequest)
    {
        switch (dataTypeID)
        {
            // For REQUEST, we use TYPE_REQUEST_SIGNATURE
            #define DISPATCH_ACCEPT_CASE_REQUEST(TYPE) \
                case TYPE##_ID: \
                    *crcSignature = TYPE##_REQUEST_SIGNATURE; \
                    return true;

            #define DISPATCH_ACCEPT_CASE_RESPONSE(TYPE)  /* nothing */
            #define DISPATCH_ACCEPT_CASE_BROADCAST(TYPE) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_ACCEPT_CASE_##KIND(TYPE)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_ACCEPT_CASE_REQUEST
            #undef DISPATCH_ACCEPT_CASE_RESPONSE
            #undef DISPATCH_ACCEPT_CASE_BROADCAST

            default:
                return false;
        }
    }

    // --------- BROADCAST ----------
    if (transferType == CanardTransferTypeBroadcast)
    {
        switch (dataTypeID)
        {
            // For BROADCAST we usually just have TYPE_SIGNATURE
            #define DISPATCH_ACCEPT_CASE_BROADCAST(TYPE) \
                case TYPE##_ID: \
                    *crcSignature = TYPE##_SIGNATURE; \
                    return true;

            #define DISPATCH_ACCEPT_CASE_REQUEST(TYPE)  /* nothing */
            #define DISPATCH_ACCEPT_CASE_RESPONSE(TYPE) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_ACCEPT_CASE_##KIND(TYPE)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_ACCEPT_CASE_BROADCAST
            #undef DISPATCH_ACCEPT_CASE_REQUEST
            #undef DISPATCH_ACCEPT_CASE_RESPONSE

            default:
                return false;
        }
    }

    return false;
}

static void onTransferReceived(CanardInstance* ins, CanardRxTransfer* transfer)
{
    // --------- RESPONSE ----------
    if (transfer->transfer_type == CanardTransferTypeResponse)
    {
        switch (transfer->data_type_id)
        {
            #define DISPATCH_HANDLER_RESPONSE(TYPE, HANDLER) \
                case TYPE##_ID: \
                    HANDLER(ins, transfer); \
                    return;

            #define DISPATCH_HANDLER_REQUEST(TYPE, HANDLER)  /* nothing */
            #define DISPATCH_HANDLER_BROADCAST(TYPE, HANDLER) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_HANDLER_##KIND(TYPE, HANDLER)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_HANDLER_RESPONSE
            #undef DISPATCH_HANDLER_REQUEST
            #undef DISPATCH_HANDLER_BROADCAST

            default:
                return;
        }
    }

    // --------- REQUEST ----------
    if (transfer->transfer_type == CanardTransferTypeRequest)
    {
        switch (transfer->data_type_id)
        {
            #define DISPATCH_HANDLER_REQUEST(TYPE, HANDLER) \
                case TYPE##_ID: \
                    HANDLER(ins, transfer); \
                    return;

            #define DISPATCH_HANDLER_RESPONSE(TYPE, HANDLER)  /* nothing */
            #define DISPATCH_HANDLER_BROADCAST(TYPE, HANDLER) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_HANDLER_##KIND(TYPE, HANDLER)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_HANDLER_REQUEST
            #undef DISPATCH_HANDLER_RESPONSE
            #undef DISPATCH_HANDLER_BROADCAST

            default:
                return;
        }
    }

    // --------- BROADCAST ----------
    if (transfer->transfer_type == CanardTransferTypeBroadcast)
    {
        switch (transfer->data_type_id)
        {
            #define DISPATCH_HANDLER_BROADCAST(TYPE, HANDLER) \
                case TYPE##_ID: \
                    HANDLER(ins, transfer); \
                    return;

            #define DISPATCH_HANDLER_REQUEST(TYPE, HANDLER)  /* nothing */
            #define DISPATCH_HANDLER_RESPONSE(TYPE, HANDLER) /* nothing */

            #define REGISTER_RX_HANDLER(TYPE, HANDLER, KIND) \
                DISPATCH_HANDLER_##KIND(TYPE, HANDLER)

            RX_HANDLER_LIST_PRIVATE

            #undef REGISTER_RX_HANDLER
            #undef DISPATCH_HANDLER_BROADCAST
            #undef DISPATCH_HANDLER_REQUEST
            #undef DISPATCH_HANDLER_RESPONSE

            default:
                return;
        }
    }
}

// === rx dronecan ===
// uavcan.protocol.getnodeinfo
static void handleGetNodeInfo(CanardInstance* ins, CanardRxTransfer *transfer) // TODO: really no reason that nodeInfoRes can't be static and not regenerated everytime
{
    PINECAN_UNUSED(ins);

    data.nodeStatus->uptime_sec = getUptimeMs() / 1000U;

    struct uavcan_protocol_GetNodeInfoResponse nodeInfoRes = {0};
    nodeInfoRes.status                                            = *(data.nodeStatus);
    nodeInfoRes.software_version.major                            = SOFTWARE_MAJOR_VERSION;
    nodeInfoRes.software_version.minor                            = SOFTWARE_MINOR_VERSION;
    nodeInfoRes.software_version.optional_field_flags             = UAVCAN_PROTOCOL_SOFTWAREVERSION_OPTIONAL_FIELD_FLAG_VCS_COMMIT;
    nodeInfoRes.software_version.vcs_commit                       = COMMIT_HASH;
    nodeInfoRes.hardware_version.major                            = HARDWARE_MAJOR_VERSION;
    nodeInfoRes.hardware_version.minor                            = HARDWARE_MINOR_VERSION;
    nodeInfoRes.hardware_version.certificate_of_authenticity.len  = 0;
    nodeInfoRes.name.len                                          = sizeof(NODE_NAME);
    memcpy(nodeInfoRes.hardware_version.unique_id, getUniqueHardwareID(), 16);

    static_assert(sizeof(NODE_NAME) <= 80, "NODE_NAME is too long to fit in GetNodeInfoResponse");
    strncpy((char*)nodeInfoRes.name.data, NODE_NAME, 79); // copy at most 79 characters to ensure null termination
    ((char*)nodeInfoRes.name.data)[79] = '\0';

    uint8_t txBuffer[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE] = {0};
    const uint32_t dataLength = uavcan_protocol_GetNodeInfoResponse_encode(&nodeInfoRes, txBuffer);

    static uint8_t transferID = 0;
    CanardTxTransfer txFrame = {
        CanardTransferTypeResponse,
        UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_SIGNATURE,
        UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_ID,
        &transferID,
        CANARD_TRANSFER_PRIORITY_LOW,
        txBuffer,
        dataLength
    };
    
    int16_t retVal = canardRequestOrRespondObj(data.canard, transfer->source_node_id, &txFrame);
    PINECAN_ASSERT(CANARD_OK == retVal);
}

// uavcan.protocol.nodestatus
static PineCAN_Status sendNodeStatus(void)
{
    data.nodeStatus->uptime_sec = getUptimeMs() / 1000U;

    uint8_t txBuffer[UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE] = {0};
    const uint32_t dataLength = uavcan_protocol_NodeStatus_encode(data.nodeStatus, txBuffer);

    static uint8_t transferID = 0;
    CanardTxTransfer txFrame = {
        CanardTransferTypeBroadcast,
        UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE,
        UAVCAN_PROTOCOL_NODESTATUS_ID,
        &transferID,
        CANARD_TRANSFER_PRIORITY_LOW,
        txBuffer,
        dataLength
    };
    int16_t retVal = canardBroadcastObj(data.canard, &txFrame);
    PINECAN_ASSERT(CANARD_OK == retVal);
    return (retVal == CANARD_OK) ? PINECAN_OK : PINECAN_ERROR;
}

static void processCanardTxQueue(void) {
    while (1) {
        const CanardCANFrame *txFrame = canardPeekTxQueue(data.canard);
        if (txFrame == NULL) {
            break;
        }

        if (txFrame->id & CANARD_CAN_FRAME_ERR) {
            // error frame, pop and continue
            canardPopTxQueue(data.canard);
        } else {
            if (processTxMessage(txFrame)) {
                // successfully transmitted, pop and continue
                canardPopTxQueue(data.canard);
            } else {
                // failed to transmit, exit and try transmitting later
                break;
            }
        }
    }
}

/* ============ INTERNAL PUBLIC FUNCTION DEFINITIONS ============ */

void init(CommonInitParams *initParams) {
    data.canard = initParams->canard;
    data.nodeStatus = initParams->nodeStatus;

    // configure Canard
    canardInit(data.canard,
        canardMemPool,
        sizeof(canardMemPool),
        onTransferReceived,
        shouldAcceptTransfer,
        NULL
    );

    canardSetLocalNodeID(data.canard, NODE_ID);
}

void handleRxFrame(CanardCANFrame *rxFrame) {
    // TODO: this rx frame is deleted after this function returns. If/when adding a queue, make sure to handle this correctly
    int16_t retVal = canardHandleRxFrame(data.canard, rxFrame, getUptimeMs() * 1000U);
    PINECAN_ASSERT(CANARD_OK == retVal);
}

/*
* @brief Passes frame to CanardHandleRxFrame.
* @param CanardCANFrame: Frame to be processed.
* @returns : N/A
*/

// queue functions for circular buffer, included in .h

bool enqueueRxQueue(const CanardCANFrame *frame) // only called inside ISR
{
    if (rxQueue.count >= RX_QUEUE_SIZE) return false; // queue full
    
    rxQueue.buffer[rxQueue.back] = *frame;     // copy entire frame
    rxQueue.back = (rxQueue.back + 1) % RX_QUEUE_SIZE;
    rxQueue.count++;

    return true;
}

/*
* @brief Add canard frame to queue
* @param CanardCANFrame*: Frame to enqueue
* @returns bool: True if successful. False if unable to push to queue
*/

CanardCANFrame* dequeueRxQueue() // only called outside ISR
{
    if (rxQueue.count == 0) return NULL; // queue empty

    CanardCANFrame* frame = &rxQueue.buffer[rxQueue.front];
    rxQueue.front = (rxQueue.front + 1) % RX_QUEUE_SIZE;
    rxQueue.count--;

    return frame;
}

/*
* @brief Remove canard frame from queue
* @param CanardCANFrame*: Frame to dequeue
* @returns bool: True if successful. False if unable to dequeue
*/

CanardCANFrame* peekRxQueue() // only called outside ISR
{
    if (rxQueue.count == 0) return NULL; // queue empty

    return &rxQueue.buffer[rxQueue.front];
}

/*
* @brief View frame at front of queue
* @param : N/A
* @returns CanardCANFrame*: Frame at front of queue. False if queue is empty
*/

void processCanardRxQueue()
{
    uint8_t queueSize = rxQueue.count;
    for (uint8_t i = 0; i < queueSize; i++) {
        // insert canard frame here
        CanardCANFrame* frame = dequeueRxQueue();
        if (frame != NULL) handleRxFrame(frame);
        return;
    }
}

/*
* @brief Passes front of queue to handleRxFrame and dequeues.
* @param : N/A
* @returns : N/A
*/

/* ============ EXTERNAL PUBLIC FUNCTION DEFINITIONS ============ */

PineCAN_Status pinecan1ms(void){
    static uint32_t nextRunTime1Hz = 0U;
    static uint32_t nextRunTimeNodeStatus = 0U;
    uint32_t uptimeMs = getUptimeMs();

    PineCAN_Status retVal = PINECAN_OK;
    
    processCanardRxQueue();
    processCanardTxQueue();

    if(uptimeMs >= nextRunTime1Hz)
    {
        nextRunTime1Hz = uptimeMs + 1000U;

        canardCleanupStaleTransfers(data.canard, getUptimeMs() * 1000U);
    }

    if (uptimeMs >= nextRunTimeNodeStatus)
    {
        nextRunTimeNodeStatus = uptimeMs + UAVCAN_PROTOCOL_NODESTATUS_MAX_BROADCASTING_PERIOD_MS/2;

        retVal |= sendNodeStatus();
    }

    return retVal;
}
