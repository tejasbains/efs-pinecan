
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <canard.h>




#define WARG_ASSERTMASTER_MAX_SIZE 0
#define WARG_ASSERTMASTER_SIGNATURE (0xA9F5212CA929ADEAULL)






struct warg_AssertMaster {




};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t warg_AssertMaster_encode(struct warg_AssertMaster* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool warg_AssertMaster_decode(const CanardRxTransfer* transfer, struct warg_AssertMaster* msg);

#if defined(CANARD_DSDLC_INTERNAL)

static inline void _warg_AssertMaster_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_AssertMaster* msg, bool tao);
static inline bool _warg_AssertMaster_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_AssertMaster* msg, bool tao);
void _warg_AssertMaster_encode(uint8_t* buffer, uint32_t* bit_ofs, struct warg_AssertMaster* msg, bool tao) {

    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;





}

/*
 decode warg_AssertMaster, return true on failure, false on success
*/
bool _warg_AssertMaster_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct warg_AssertMaster* msg, bool tao) {

    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;



    return false; /* success */

}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct warg_AssertMaster sample_warg_AssertMaster_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>

#endif
#endif
