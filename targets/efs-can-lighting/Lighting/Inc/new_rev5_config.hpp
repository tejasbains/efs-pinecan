/*
 * new_rev5_config.hpp
 *
 * rev5 board facts (see LIGHTING_MIGRATION_SPEC.md)
 *
 *  Author: Avi
 */

#ifndef INC_NEW_REV5_CONFIG_HPP_
#define INC_NEW_REV5_CONFIG_HPP_

#include <cstdint>

#include "conversions.hpp"

#define BOARD_REV 5

// Positions -- three concentric rings of four:
//   SIDE_NW=0  OUTER_NW=1  INNER_NW=2  INNER_SW=3  OUTER_SW=4  SIDE_SW=5
//   SIDE_SE=6  OUTER_SE=7  INNER_SE=8  INNER_NE=9  OUTER_NE=10 SIDE_NE=11
static constexpr uint8_t NUM_LEDS = 12;

// idx 0,5,6,11 = CHIP_RGB (legacy SK6812) | idx 1,2,3,4,7,8,9,10 = CHIP_RGBW
static constexpr ChipType led_types[NUM_LEDS] = {
	CHIP_RGB,  CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, 
	CHIP_RGBW, CHIP_RGB,  CHIP_RGB,  CHIP_RGBW,  
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGB    
};

static constexpr uint8_t  NUM_LEDS_PADDING = 10;
static constexpr uint16_t PADDING_SIZE     = NUM_LEDS_PADDING * 32;


static constexpr uint16_t BANK_OUTPUT_BUFFER_SIZE = 4 * 24 + 8 * 32 + PADDING_SIZE;
static constexpr uint16_t DMA_OUTPUT_BUFFER_SIZE  = BANK_OUTPUT_BUFFER_SIZE * 2;


static constexpr uint16_t zone_leds[CD_LENGTH] = {
	/* CD_MAIN    */ 0x0FFF,  // all 12
	/* CD_TAXI    */ 0x0861,  // SIDE  ring {0,5,6,11}
	/* CD_LANDING */ 0x0492,  // OUTER ring {1,4,7,10}
	/* CD_NAV     */ 0x0861,  // SIDE  ring {0,5,6,11}
	/* CD_BEACON  */ 0x0492,  // OUTER ring {1,4,7,10}
	/* CD_STROBE  */ 0x030C,  // INNER ring {2,3,8,9}
	/* CD_BRAKE   */ 0x030C,  // INNER ring {2,3,8,9}
	/* CD_SEARCH  */ 0x030C,  // INNER ring {2,3,8,9}  
};

#define LED_TIM          htim2
#define LED_TIM_CHANNEL  TIM_CHANNEL_1

#endif 
