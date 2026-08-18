#include "can_manager.hpp"
#include "can.h"

// Pure decode/forwarding core (no PineCAN/HAL state); see Lighting/Lighting.md (Decode).

// Forwards to canService().
void CANManager::service() {
    canService();
}

// Forwards to canGetLatestVehicleState(); no mapping.
uint64_t CANManager::getLatestVehicleState() {
    return canGetLatestVehicleState();
}

// True if raw_vehicle_state differs from the previous call; first call returns false (baseline).
uint64_t CANManager::prev_data_ = 0;
bool     CANManager::have_prev_ = false;

bool CANManager::vehicleStateChanged(uint64_t raw_vehicle_state) {
    if (!have_prev_) {
        prev_data_ = raw_vehicle_state;
        have_prev_ = true;
        return false;
    }

    if (raw_vehicle_state != prev_data_) {
        prev_data_ = raw_vehicle_state;
        return true;
    }
    return false;
}

// Pure map from a 64-bit vehicle_state bitmask to a LightingStateTransition (bits/precedence in Lighting.md).
uint8_t CANManager::mapVehicleState(uint64_t vehicle_state) {
    if (vehicle_state == 0) {
        return static_cast<uint8_t>(TRANSITION_GROUND);
    }

    constexpr uint64_t MAPPED_BITS =
        (1ULL << 23) | (1ULL << 22) | (1ULL << 2) | (1ULL << 1) | (1ULL << 0);

    if ((vehicle_state & MAPPED_BITS) == 0) {
        return UNRECOGNIZED_STATE;
    }

    if (vehicle_state & (1ULL << 23)) return static_cast<uint8_t>(TRANSITION_TAKEOFF);
    if (vehicle_state & (1ULL << 22)) return static_cast<uint8_t>(TRANSITION_LANDING);
    if (vehicle_state & (1ULL <<  2)) return static_cast<uint8_t>(TRANSITION_FLIGHT);
    if (vehicle_state & (1ULL <<  1)) return static_cast<uint8_t>(TRANSITION_STANDBY);
    if (vehicle_state & (1ULL <<  0)) return static_cast<uint8_t>(TRANSITION_STARTUP);

    return UNRECOGNIZED_STATE;
}

// Thin delegator to mapVehicleState; call when vehicleStateChanged() returns true.
uint8_t interpretVehicleState(uint64_t vehicle_state) {
    return CANManager::mapVehicleState(vehicle_state);
}
