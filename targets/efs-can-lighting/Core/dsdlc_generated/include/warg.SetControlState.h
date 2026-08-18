
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <canard.h>




#define WARG_SETCONTROLSTATE_MAX_SIZE 1
#define WARG_SETCONTROLSTATE_SIGNATURE (0xFA38DE03277F7E5BULL)

#define WARG_SETCONTROLSTATE_ID 800





#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
class warg_SetControlState_cxx_iface;
#endif


struct warg_SetControlState {

#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
    using cxx_iface = warg_SetControlState_cxx_iface;
#endif




    uint8_t controlState;



};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t warg_SetControlState_encode(struct warg_SetControlState* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool warg_SetControlState_decode(const CanardRxTransfer* transfer, struct warg_SetControlState* msg);

#if defined(CANARD_DSDLC_INTERNAL)

static inline void _warg_SetControlState_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_SetControlState* msg, bool tao);
static inline bool _warg_SetControlState_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_SetControlState* msg, bool tao);
void _warg_SetControlState_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_SetControlState* msg, bool tao) {

    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;






    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->controlState);

    *bit_ofs += 8;





}

/*
 decode warg_SetControlState, return true on failure, false on success
*/
bool _warg_SetControlState_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_SetControlState* msg, bool tao) {

    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;





    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->controlState);

    *bit_ofs += 8;





    return false; /* success */

}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct warg_SetControlState sample_warg_SetControlState_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>


BROADCAST_MESSAGE_CXX_IFACE(warg_SetControlState, WARG_SETCONTROLSTATE_ID, WARG_SETCONTROLSTATE_SIGNATURE, WARG_SETCONTROLSTATE_MAX_SIZE);


#endif
#endif
