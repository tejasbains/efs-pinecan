/*
 * can_manager.hpp
 *
 * Pure, transport-agnostic decode core for the CAN receive pipeline.
 * See Lighting/Lighting.md (PineCAN + Decode) for the full description.
 */

#ifndef INC_CAN_MANAGER_HPP_
#define INC_CAN_MANAGER_HPP_

#include <cstdint>
#include "conversions.hpp"

class CANManager {
public:
    /// Returned by mapVehicleState when no bit maps to a state.
    static constexpr uint8_t UNRECOGNIZED_STATE = 0xFF;

    /// Pure map from a 64-bit vehicle_state bitmask to a LightingStateTransition.
    static uint8_t mapVehicleState(uint64_t vehicle_state);

    /// Forwards to canGetLatestVehicleState(); no mapping.
    static uint64_t getLatestVehicleState();

    /// True if raw_vehicle_state differs from the previous call; first call returns false (baseline).
    static bool vehicleStateChanged(uint64_t raw_vehicle_state);

    /// Forwards to canService().
    static void service();

private:
    static uint64_t prev_data_;
    static bool     have_prev_;
};

/// Thin delegator to CANManager::mapVehicleState; call when vehicleStateChanged() returns true.
uint8_t interpretVehicleState(uint64_t vehicle_state);

#endif /* INC_CAN_MANAGER_HPP_ */
