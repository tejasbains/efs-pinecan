
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <canard.h>




#define FRED_SETCONTROLSTATE_MAX_SIZE 1
#define FRED_SETCONTROLSTATE_SIGNATURE (0x3140FBFF117B7238ULL)






struct fred_SetControlState {




    uint8_t state;



};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t fred_SetControlState_encode(struct fred_SetControlState* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool fred_SetControlState_decode(const CanardRxTransfer* transfer, struct fred_SetControlState* msg);

#if defined(CANARD_DSDLC_INTERNAL)

static inline void _fred_SetControlState_encode(uint8_t* buffer, uint32_t* bit_ofs, struct fred_SetControlState* msg, bool tao);
static inline bool _fred_SetControlState_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct fred_SetControlState* msg, bool tao);
void _fred_SetControlState_encode(uint8_t* buffer, uint32_t* bit_ofs, struct fred_SetControlState* msg, bool tao) {

    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;






    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->state);

    *bit_ofs += 8;





}

/*
 decode fred_SetControlState, return true on failure, false on success
*/
bool _fred_SetControlState_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct fred_SetControlState* msg, bool tao) {

    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;





    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->state);

    *bit_ofs += 8;





    return false; /* success */

}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct fred_SetControlState sample_fred_SetControlState_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>

#endif
#endif
