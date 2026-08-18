/*
 * debug_trace.h
 *
 * Header-only RAM trace buffer for diagnosing the CMake/CLI build.
 *
 * WHY A RAM BUFFER
 *   The failure mode wedges the CPU inside an interrupt handler, so there is no
 *   opportunity to print anything and no UART is configured on this board. Instead
 *   every milestone is recorded into a fixed address in SRAM2, which can be read
 *   back over SWD *without resetting the part*:
 *
 *       cmake --build --preset=rev5-debug --target trace
 *
 *   SRAM2 is not zeroed by the startup code, so the buffer also survives a warm
 *   reset and a `magic` word is used to tell a valid buffer from power-on garbage.
 *
 * WHY HEADER-ONLY
 *   Adding a .c/.cpp file would require updating the STM32CubeIDE project, and the
 *   CubeIDE build is our known-good reference. Everything here is `static inline`,
 *   so no new translation unit is introduced. All of it compiles to nothing unless
 *   LIGHTING_TRACE is defined, which only the CMake build does, so CubeIDE builds
 *   are bit-for-bit unaffected.
 *
 * MEMORY PLACEMENT
 *   The buffer lives at the base of SRAM2 (0x10000000). Per STM32L431KCUX_FLASH.ld
 *   the linker allocates nothing to the RAM2 region (`RAM2: 0 B / 16 KB`), and the
 *   stack grows *down* from _estack = 0x20010000, which is the top of the SRAM2
 *   alias. Placing the buffer at the base of SRAM2 keeps it clear of both .bss
 *   (which currently ends well below 0x2000C000) and the stack. No linker script
 *   change is needed, which matters because that file is shared with CubeIDE.
 *n
 * CONCURRENCY
 *   Records are written with interrupts masked. Counters are plain increments and
 *   may lose a count if interrupted; that is acceptable for diagnostics and keeps
 *   them cheap enough to call from a high-rate ISR.
 */

#ifndef INC_DEBUG_TRACE_H_
#define INC_DEBUG_TRACE_H_

#include <stdint.h>
#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DBG_TRACE_BASE      0x10000000u   /* base of SRAM2, unallocated by the linker */
#define DBG_TRACE_MAGIC     0x4C494754u   /* 'LIGT' */
#define DBG_TRACE_VERSION   1u
#define DBG_TRACE_CAPACITY  128u          /* must be a power of two */

/* ---- Event ids (ring buffer) ---- */
enum DbgEvent {
    DBG_EV_NONE = 0,
    DBG_EV_BOOT,                /* a = boot_count                                  */
    DBG_EV_HAL_INIT_DONE,
    DBG_EV_CLOCK_CONFIG_DONE,   /* a = SystemCoreClock                             */
    DBG_EV_GPIO_INIT_DONE,
    DBG_EV_DMA_INIT_DONE,
    DBG_EV_CAN1_INIT_DONE,
    DBG_EV_TIM1_INIT_DONE,
    DBG_EV_TIM6_INIT_DONE,
    DBG_EV_TIM7_INIT_DONE,
    DBG_EV_TIM2_INIT_DONE,
    DBG_EV_TIM6_START,          /* a = HAL status                                  */
    DBG_EV_TIM2_START,          /* a = HAL status                                  */
    DBG_EV_NODE_ID_DONE,        /* a = node_id                                     */
    DBG_EV_INITCAN_ENTER,
    DBG_EV_INITCAN_RESULT,      /* a = PineCAN_Status                              */
    DBG_EV_LED_INIT_ENTER,
    DBG_EV_PWM_DMA_RESULT,      /* a = HAL status, b = DMA length                  */
    DBG_EV_LED_INIT_DONE,
    DBG_EV_MAIN_LOOP_ENTER,
    DBG_EV_FIRST_GENERATE,      /* a = tick                                        */
    DBG_EV_FIRST_PUSH,
    DBG_EV_SELECT_PATTERN,      /* a = flight_state                                */
    DBG_EV_STATE_CHANGED,       /* a = vehicle_state low32, b = high32             */
    DBG_EV_NOTIFY_RX,           /* a = decode result (0 = success)                 */
    DBG_EV_NOTIFY_STATE,        /* a = vehicle_state low32, b = high32             */
    DBG_EV_HEARTBEAT,           /* a = tick, b = inner loop iterations             */
    DBG_EV_ERROR_HANDLER,       /* HAL Error_Handler() reached                      */
    DBG_EV_ASSERT,              /* a = line, b = expr string pointer               */
    DBG_EV_NMI,
    DBG_EV_HARDFAULT,
    DBG_EV_MEMMANAGE,
    DBG_EV_BUSFAULT,
    DBG_EV_USAGEFAULT,
    DBG_EV_MAX
};

/* ---- Counters (high-rate events; index into counters[]) ---- */
enum DbgCounter {
    DBG_CNT_SYSTICK = 0,
    DBG_CNT_DMA_IRQ,        /* DMA1_Channel5_IRQHandler entries                    */
    DBG_CNT_DMA_HALF,       /* PWM half-transfer callback  -> DMA is streaming      */
    DBG_CNT_DMA_FULL,       /* PWM transfer-complete callback                       */
    DBG_CNT_TIM1_IRQ,
    DBG_CNT_TIM2_IRQ,       /* expect a storm: TIM2 period 96 @ 48 MHz ~= 500 kHz   */
    DBG_CNT_TIM6_IRQ,
    DBG_CNT_TIM7_IRQ,
    DBG_CNT_CAN_RX_IRQ,     /* CAN1_RX0_IRQHandler entries                          */
    DBG_CNT_NOTIFY_OK,
    DBG_CNT_NOTIFY_FAIL,
    DBG_CNT_INNER_LOOP,
    DBG_CNT_GENERATE,
    DBG_CNT_PUSH,
    DBG_CNT_SELECT,
    DBG_CNT_SERVICE,
    DBG_CNT_COUNT           /* keep last; must be <= DBG_TRACE_COUNTERS             */
};

#define DBG_TRACE_COUNTERS 16u

typedef struct {
    uint32_t tick;      /* HAL_GetTick() at the time of the record */
    uint16_t id;        /* enum DbgEvent                           */
    uint16_t seq;       /* low 16 bits of the global write index    */
    uint32_t a;
    uint32_t b;
} DbgTraceRecord;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint32_t write_index;                    /* total records ever written        */
    uint32_t capacity;
    uint32_t last_event;
    uint32_t counters[DBG_TRACE_COUNTERS];
    /* fault[] : 0 = event id, 1 = CFSR, 2 = HFSR, 3 = BFAR,
     *           4 = MMFAR,    5 = MSP,  6 = DFSR, 7 = SHCSR      */
    uint32_t fault[8];
    uint32_t fault_stack[8];                 /* 8 words from MSP at fault time    */
    DbgTraceRecord records[DBG_TRACE_CAPACITY];
} DbgTrace;

#define DBG_TRACE ((volatile DbgTrace *)DBG_TRACE_BASE)

/* Total bytes to read back over SWD. */
#define DBG_TRACE_BYTES ((uint32_t)sizeof(DbgTrace))

static inline void dbgTraceReset(void)
{
    volatile DbgTrace *t = DBG_TRACE;
    uint32_t i;

    t->magic       = DBG_TRACE_MAGIC;
    t->version     = DBG_TRACE_VERSION;
    t->write_index = 0u;
    t->capacity    = DBG_TRACE_CAPACITY;
    t->last_event  = DBG_EV_NONE;

    for (i = 0u; i < DBG_TRACE_COUNTERS; ++i) {
        t->counters[i] = 0u;
    }
    for (i = 0u; i < 8u; ++i) {
        t->fault[i]       = 0u;
        t->fault_stack[i] = 0u;
    }
    for (i = 0u; i < DBG_TRACE_CAPACITY; ++i) {
        t->records[i].tick = 0u;
        t->records[i].id   = DBG_EV_NONE;
        t->records[i].seq  = 0u;
        t->records[i].a    = 0u;
        t->records[i].b    = 0u;
    }
}

/* Call once, as early as possible in main(). Increments boot_count across resets. */
static inline void dbgTraceInit(void)
{
    volatile DbgTrace *t = DBG_TRACE;
    uint32_t boots = 0u;

    if (t->magic == DBG_TRACE_MAGIC && t->version == DBG_TRACE_VERSION) {
        boots = t->boot_count;   /* warm reset: keep the boot counter running */
    }

    dbgTraceReset();
    t->boot_count = boots + 1u;
}

static inline void dbgTraceEvent(uint16_t id, uint32_t a, uint32_t b)
{
    volatile DbgTrace *t = DBG_TRACE;
    uint32_t primask;
    uint32_t idx;

    if (t->magic != DBG_TRACE_MAGIC) {
        dbgTraceInit();
    }

    primask = __get_PRIMASK();
    __disable_irq();

    idx = t->write_index;
    t->records[idx & (DBG_TRACE_CAPACITY - 1u)].tick = HAL_GetTick();
    t->records[idx & (DBG_TRACE_CAPACITY - 1u)].id   = id;
    t->records[idx & (DBG_TRACE_CAPACITY - 1u)].seq  = (uint16_t)idx;
    t->records[idx & (DBG_TRACE_CAPACITY - 1u)].a    = a;
    t->records[idx & (DBG_TRACE_CAPACITY - 1u)].b    = b;
    t->write_index = idx + 1u;
    t->last_event  = id;

    if (primask == 0u) {
        __enable_irq();
    }
}

/* Cheap, ISR-safe enough for high-rate use. */
static inline void dbgTraceCount(uint32_t counter)
{
    if (counter < DBG_TRACE_COUNTERS) {
        DBG_TRACE->counters[counter]++;
    }
}

/* Snapshot the Cortex-M fault status registers and the exception stack frame. */
static inline void dbgTraceFault(uint16_t id)
{
    volatile DbgTrace *t = DBG_TRACE;
    uint32_t msp = __get_MSP();
    const uint32_t *sp = (const uint32_t *)msp;
    uint32_t i;

    t->fault[0] = id;
    t->fault[1] = SCB->CFSR;
    t->fault[2] = SCB->HFSR;
    t->fault[3] = SCB->BFAR;
    t->fault[4] = SCB->MMFAR;
    t->fault[5] = msp;
    t->fault[6] = SCB->DFSR;
    t->fault[7] = SCB->SHCSR;

    for (i = 0u; i < 8u; ++i) {
        t->fault_stack[i] = sp[i];
    }

    dbgTraceEvent(id, SCB->CFSR, msp);
}

#ifdef __cplusplus
}
#endif

/* ---- Public macros: no-ops unless LIGHTING_TRACE is defined ---- */
#if defined(LIGHTING_TRACE) && (LIGHTING_TRACE)
#  define DBG_INIT()          dbgTraceInit()
#  define DBG_EV(id, a, b)    dbgTraceEvent((uint16_t)(id), (uint32_t)(a), (uint32_t)(b))
#  define DBG_CNT(c)          dbgTraceCount((uint32_t)(c))
#  define DBG_FAULT(id)       dbgTraceFault((uint16_t)(id))
#else
#  define DBG_INIT()          ((void)0)
#  define DBG_EV(id, a, b)    ((void)0)
#  define DBG_CNT(c)          ((void)0)
#  define DBG_FAULT(id)       ((void)0)
#endif

#endif /* INC_DEBUG_TRACE_H_ */
