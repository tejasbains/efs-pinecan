/*
 * new_rev4_config.hpp
 *
 * rev4 board facts (see LIGHTING_MIGRATION_SPEC.md)
 *
 *  Author: Avi
 */

#ifndef INC_NEW_REV4_CONFIG_HPP_
#define INC_NEW_REV4_CONFIG_HPP_

#include <cstdint>

#include "conversions.hpp"

#define BOARD_REV 4

// Positions:
//   NW=0 W=1 SW=2 SE=3 E=4 NE=5 SIDE_NE=6 SIDE_SE=7 SIDE_SW=8 SIDE_NW=9
static constexpr uint8_t NUM_LEDS = 10;

// ALL 10 = CHIP_RGBW (legacy WS2812)
static constexpr ChipType led_types[NUM_LEDS] = {
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW,
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW
};

static constexpr uint8_t  NUM_LEDS_PADDING = 10;
static constexpr uint16_t PADDING_SIZE     = NUM_LEDS_PADDING * 32;

static constexpr uint16_t BANK_OUTPUT_BUFFER_SIZE = NUM_LEDS * 32 + PADDING_SIZE;
static constexpr uint16_t DMA_OUTPUT_BUFFER_SIZE  = BANK_OUTPUT_BUFFER_SIZE * 2;


static constexpr uint16_t zone_leds[CD_LENGTH] = {
	/* CD_MAIN    */ 0x03FF,  // all 10
	/* CD_TAXI    */ 0x03C0,  // sides   {6,7,8,9}
	/* CD_LANDING */ 0x002D,  // corners {0,2,3,5}
	/* CD_NAV     */ 0x03C0,  // sides   {6,7,8,9}
	/* CD_BEACON  */ 0x002D,  // corners {0,2,3,5}
	/* CD_STROBE  */ 0x0012,  // E/W     {1,4}  
	/* CD_BRAKE   */ 0x0012,  // E/W     {1,4}   
	/* CD_SEARCH  */ 0x003F,  // {0,1,2,3,4,5}
};



#define LED_TIM          htim2
#define LED_TIM_CHANNEL  TIM_CHANNEL_1

#endif 
