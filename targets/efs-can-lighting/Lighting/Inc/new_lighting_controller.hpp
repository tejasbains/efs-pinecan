/*
 * new_lighting_controller.hpp
 *
 * Public entry points for the PineCAN-driven lighting pipeline (see
 * LIGHTING_MIGRATION_SPEC.md §9). Three separate functions, no 3-in-1
 * wrapper — the main loop's placeholder split (outer: on state change only;
 * inner: every rate-gated iteration) requires it.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_LIGHTING_CONTROLLER_HPP_
#define INC_NEW_LIGHTING_CONTROLLER_HPP_

#include <cstdint>

#include "conversions.hpp"
#include "tim.h"

//   CHIP_RGB   <- legacy class `SK6812`  (3 channels, 24 bits)
//   CHIP_RGBW  <- legacy class `WS2812`  (4 channels, 32 bits)
enum ChipType {
	CHIP_RGB  = 0,   // 3 channels, 24 bits
	CHIP_RGBW = 1    // 4 channels, 32 bits
};

// The #if selects which config header is included -- nothing else.
// Board-specific facts (LED count, chip types, zone->LED membership, buffer
// sizes, timer/channel) live entirely in the selected config file.
#if defined(REV5)
#include "new_rev5_config.hpp"
#elif defined(REV4)
#include "new_rev4_config.hpp"
#endif

void led_init();

/**
 * @param flight_state : a LightingStateTransition value (0-8), or 0xFF
 */
void Select_Pattern(uint8_t flight_state);

/**
 * Advances animations for this tick and expands the active zones into their
 * physical LEDs, writing the per-LED colour buffer used by Push_Leds.
 *
 * Call from main's INNER loop -- every iteration (rate-gated, ~50 Hz).
 *
 * @param tick : monotonically increasing tick counter, incremented once per
 *               rate-gated call by the caller
 */
void Generate_Leds(uint32_t tick);


void Push_Leds();

#endif /* INC_NEW_LIGHTING_CONTROLLER_HPP_ */
