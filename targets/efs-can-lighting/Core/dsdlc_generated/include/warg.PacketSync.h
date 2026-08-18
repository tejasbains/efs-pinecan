
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <canard.h>




#define WARG_PACKETSYNC_MAX_SIZE 0
#define WARG_PACKETSYNC_SIGNATURE (0x1B0EC337E05F38E4ULL)






struct warg_PacketSync {




};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t warg_PacketSync_encode(struct warg_PacketSync* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool warg_PacketSync_decode(const CanardRxTransfer* transfer, struct warg_PacketSync* msg);

#if defined(CANARD_DSDLC_INTERNAL)

static inline void _warg_PacketSync_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_PacketSync* msg, bool tao);
static inline bool _warg_PacketSync_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_PacketSync* msg, bool tao);
void _warg_PacketSync_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_PacketSync* msg, bool tao) {

    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;





}

/*
 decode warg_PacketSync, return true on failure, false on success
*/
bool _warg_PacketSync_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_PacketSync* msg, bool tao) {

    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;



    return false; /* success */

}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct warg_PacketSync sample_warg_PacketSync_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>

#endif
#endif
