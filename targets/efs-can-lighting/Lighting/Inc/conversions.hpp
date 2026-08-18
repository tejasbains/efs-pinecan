/*
 * conversions.hpp
 *
 *  Created on: Nov 21, 2024
 *      Author: Anni
 */

#ifndef INC_CONVERSIONS_HPP_
#define INC_CONVERSIONS_HPP_

#include <cstdint>

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} RGB_colour_t;


//list of colours
constexpr RGB_colour_t WHITE = { 255, 255, 255 };
constexpr RGB_colour_t RED = { 255, 0, 0 };
constexpr RGB_colour_t ORANGE = {255, 165, 0};
constexpr RGB_colour_t GREEN = {0, 255, 0};
constexpr RGB_colour_t CYAN = {0, 255, 255};
constexpr RGB_colour_t BROWN = {139, 69, 19};
constexpr RGB_colour_t PURPLE = {255, 0, 255};
constexpr RGB_colour_t BLUE = {0, 0, 255};

typedef struct {
	uint16_t hue;		// TODO: limit this to 0-360 (degrees)
	uint8_t saturation;	// TODO: limit this to 0-100 (percentage)
	uint8_t lightness; 	// TODO: limit this to 0-100 (percentage)
} HSL_colour_t;


/**
 * @enum ControlDomain
 * Represents the different command mode that an LED might be responsive to.
 *
 * CD_MAIN - default mode for LED's
 * CD_TAXI - white light for when the drone is in taxi.
 * CD_LANDING - bright white lights, indicates the drone is landing
 * CD_NAVIGATION - green lights for when the drone is in flight, used for visibility.
 * CD_BEACON - beacon lights (red pulse 1x per second), indicates LV ON
 * CD_STROBE - strobe lights (Double white flash 1x per second), indicates HV ON
 * CD_Brake - orange lights for when the drone stops.
 * CD_SEARCH - scrolling pattern used for when CAN messages are received but are no longer available.
 * Add other modes as necessary
 */


enum ControlDomain {
	CD_MAIN = 0,
	CD_TAXI = 1,
	CD_LANDING = 2,
	CD_NAV = 3,
	CD_BEACON = 4,
	CD_STROBE = 5,
	CD_BRAKE = 6,
	CD_SEARCH = 7,
	CD_LENGTH = 8 // increment this as necessary
};


/* For transitioning to lighting control states (?):
*/

enum LightingStateTransition {
	TRANSITION_GROUND = 0,
	TRANSITION_STANDBY = 1,
	TRANSITION_TAXI = 2,
	TRANSITION_TAKEOFF = 3,
	TRANSITION_FLIGHT = 4,
	TRANSITION_BRAKE = 5,
	TRANSITION_LANDING = 6,
	TRANSITION_SEARCH = 7,
	TRANSITION_STARTUP = 8
};

#endif /* INC_CONVERSIONS_HPP_ */
