
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <canard.h>




#define FRED_MYPACKET_MAX_SIZE 1
#define FRED_MYPACKET_SIGNATURE (0xFDBECAE857DBE334ULL)






struct fred_MyPacket {




    uint8_t mynumber;



};

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t fred_MyPacket_encode(struct fred_MyPacket* msg, uint8_t* buffer
#if CANARD_ENABLE_TAO_OPTION
    , bool tao
#endif
);
bool fred_MyPacket_decode(const CanardRxTransfer* transfer, struct fred_MyPacket* msg);

#if defined(CANARD_DSDLC_INTERNAL)

static inline void _fred_MyPacket_encode(uint8_t* buffer, uint32_t* bit_ofs, struct fred_MyPacket* msg, bool tao);
static inline bool _fred_MyPacket_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct fred_MyPacket* msg, bool tao);
void _fred_MyPacket_encode(uint8_t* buffer, uint32_t* bit_ofs, struct fred_MyPacket* msg, bool tao) {

    (void)buffer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;






    canardEncodeScalar(buffer, *bit_ofs, 8, &msg->mynumber);

    *bit_ofs += 8;





}

/*
 decode fred_MyPacket, return true on failure, false on success
*/
bool _fred_MyPacket_decode(const CanardRxTransfer* transfer, uint32_t* bit_ofs, struct fred_MyPacket* msg, bool tao) {

    (void)transfer;
    (void)bit_ofs;
    (void)msg;
    (void)tao;





    canardDecodeScalar(transfer, *bit_ofs, 8, false, &msg->mynumber);

    *bit_ofs += 8;





    return false; /* success */

}
#endif
#ifdef CANARD_DSDLC_TEST_BUILD
struct fred_MyPacket sample_fred_MyPacket_msg(void);
#endif
#ifdef __cplusplus
} // extern "C"

#ifdef DRONECAN_CXX_WRAPPERS
#include <canard/cxx_wrappers.h>

#endif
#endif
