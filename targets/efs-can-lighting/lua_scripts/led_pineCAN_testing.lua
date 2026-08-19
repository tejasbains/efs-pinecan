-- led_pineCAN_testing.lua
--
-- TESTING variant of led_pineCAN.lua. Instead of reading real ArduPilot
-- flight data, it cycles through every flight-phase state once per second and
-- broadcasts the corresponding `ardupilot.indication.NotifyState` DroneCAN
-- message (data type id 20007). The wire output is identical to
-- led_pineCAN.lua -- the ONLY difference is where the state comes from and the
-- 1 Hz cadence -- so it exercises the efs-can-lighting board's handleNotifyState
-- + mapVehicleState pipeline through all lighting transitions without a vehicle.
--
-- Do not fly with this script; it ignores actual arming/flight state.

-- ---------------------------------------------------------------------------
-- Configuration constants
-- ---------------------------------------------------------------------------
local NOTIFYSTATE_ID   = 20007      -- ardupilot.indication.NotifyState data type id
local PRIORITY         = 16         -- DroneCAN priority (0 highest, 31 lowest)
local SOURCE_NODE_ID   = 11         -- this Lua sender's node id (distinct from board NODE_ID 71)
local PERIOD_MS        = 5000       -- step to the next state every 1 second
local WRITE_TIMEOUT_US = 10000      -- per-frame write timeout in microseconds
local CAN_DEVICE_INDEX = 5          -- CAN scripting driver buffer index

-- ardupilot.indication.NotifyState vehicle_state bit positions (must match
-- can_manager.cpp::mapVehicleState on the board).
local BIT = {
    INITIALISING  = 0,   -- -> TRANSITION_STARTUP (8), lowest priority
    ARMED         = 1,   -- -> TRANSITION_STANDBY (1)
    FLYING        = 2,   -- -> TRANSITION_FLIGHT  (4)
    IS_LANDING    = 22,  -- -> TRANSITION_LANDING (6)
    IS_TAKING_OFF = 23,  -- -> TRANSITION_TAKEOFF (3), highest priority
}

-- Flight-phase states, same values as led_pineCAN.lua / led_can.lua.
local states = {
    startup = 8,
    ground  = 0,
    taxi    = 2,
    takeoff = 3,
    flight  = 4,
    landing = 6
}

-- Order in which the testing loop steps through the states.
local state_list = {
    states.startup,
    states.ground,
    states.taxi,
    states.takeoff,
    states.flight,
    states.landing
}

-- ARDUPILOT_INDICATION_NOTIFYSTATE_SIGNATURE = 0x631F2A9C1651FDEC, little-endian.
local SIGNATURE_BYTES = { 0xEC, 0xFD, 0x51, 0x16, 0x9C, 0x2A, 0x1F, 0x63 }

-- ---------------------------------------------------------------------------
-- CAN device acquisition
-- ---------------------------------------------------------------------------
local device = CAN:get_device(CAN_DEVICE_INDEX)
if device == nil then
    gcs:send_text(3, "led_pineCAN_testing: CAN device not found!")
    return
end

local transfer_id = 0
local state_index = 1                 -- cycles 1..#state_list

-- ---------------------------------------------------------------------------
-- Flight-State Source (TESTING) -- cycle through every state, ignoring real data
-- ---------------------------------------------------------------------------
local function calculate_current_state()
    local state = state_list[state_index]
    state_index = state_index + 1
    if state_index > #state_list then state_index = 1 end
    return state
end

-- ---------------------------------------------------------------------------
-- State Mapper -- translate the integer flight state into a 64-bit
-- vehicle_state bitmask whose set bits are exactly the flags mapVehicleState()
-- inspects. (Identical to led_pineCAN.lua.)
-- ---------------------------------------------------------------------------
local function state_to_vehicle_state(state)
    if     state == states.startup then return 1 << BIT.INITIALISING   -- bit 0
    elseif state == states.ground  then return 0                       -- vs == 0
    elseif state == states.taxi    then return 1 << BIT.ARMED          -- bit 1
    elseif state == states.takeoff then return 1 << BIT.IS_TAKING_OFF  -- bit 23
    elseif state == states.flight  then return 1 << BIT.FLYING         -- bit 2
    elseif state == states.landing then return 1 << BIT.IS_LANDING     -- bit 22
    else   return 0 end                                                -- safe default: GROUND
end

-- ---------------------------------------------------------------------------
-- Payload Encoder -- serialize NotifyState in exact DSDL encode order:
--   aux_data_type (u8), aux_data.len (u8), [aux bytes], vehicle_state (u64 LE).
-- ---------------------------------------------------------------------------
local function encode_notifystate_payload(vehicle_state)
    local p = {}
    p[#p + 1] = 0                                   -- aux_data_type = 0
    p[#p + 1] = 0                                   -- aux_data.len  = 0 (no aux bytes)
    for i = 0, 7 do                                 -- vehicle_state, little-endian
        p[#p + 1] = (vehicle_state >> (8 * i)) & 0xFF
    end
    return p                                         -- length == 10
end

-- ---------------------------------------------------------------------------
-- Transfer CRC -- CRC-16-CCITT (poly 0x1021, init 0xFFFF), MSB-first, seeded
-- with the message signature bytes then the payload bytes.
-- ---------------------------------------------------------------------------
local function crc16_add_byte(crc, b)
    crc = (crc ~ (b << 8)) & 0xFFFF
    for _ = 1, 8 do
        if (crc & 0x8000) ~= 0 then
            crc = ((crc << 1) ~ 0x1021) & 0xFFFF
        else
            crc = (crc << 1) & 0xFFFF
        end
    end
    return crc
end

local function compute_transfer_crc(payload)
    local crc = 0xFFFF
    for i = 1, #SIGNATURE_BYTES do crc = crc16_add_byte(crc, SIGNATURE_BYTES[i]) end
    for i = 1, #payload         do crc = crc16_add_byte(crc, payload[i])         end
    return crc
end

-- ---------------------------------------------------------------------------
-- Tail byte -- bit7 = start_of_transfer, bit6 = end_of_transfer,
-- bit5 = toggle, bits[4:0] = transfer_id.
-- ---------------------------------------------------------------------------
local function make_tail_byte(start_of_transfer, end_of_transfer, toggle, id)
    local b = id & 0x1F
    if start_of_transfer then b = b | 0x80 end
    if end_of_transfer   then b = b | 0x40 end
    if toggle            then b = b | 0x20 end
    return b
end

-- ---------------------------------------------------------------------------
-- Frame Splitter -- single frame when payload <= 7 bytes; else a multi-frame
-- transfer whose first frame is prefixed with the 2-byte transfer CRC (LE).
-- ---------------------------------------------------------------------------
local function split_into_frames(payload, id)
    local frames = {}

    if #payload <= 7 then
        local data = {}
        for i = 1, #payload do data[i] = payload[i] end
        data[#data + 1] = make_tail_byte(true, true, false, id)  -- SOT=1 EOT=1 toggle=0
        frames[1] = { data = data, dlc = #data }
        return frames
    end

    -- Multi-frame: prepend the 2-byte transfer CRC (little-endian).
    local crc = compute_transfer_crc(payload)
    local stream = { crc & 0xFF, (crc >> 8) & 0xFF }
    for i = 1, #payload do stream[#stream + 1] = payload[i] end

    local idx, toggle, first = 1, false, true
    while idx <= #stream do
        local data, n = {}, 0
        while n < 7 and idx <= #stream do
            n = n + 1
            data[n] = stream[idx]
            idx = idx + 1
        end
        local is_last = (idx > #stream)
        data[n + 1] = make_tail_byte(first, is_last, toggle, id)
        frames[#frames + 1] = { data = data, dlc = n + 1 }
        first  = false
        toggle = not toggle
    end
    return frames
end

-- ---------------------------------------------------------------------------
-- CAN ID Builder -- 29-bit DroneCAN message-frame id:
--   priority[28:24] | message_type_id[23:8] | service_bit[7]=0 | node_id[6:0]
-- plus bit 31 set as the ArduPilot extended-frame marker.
-- ---------------------------------------------------------------------------
local function make_message_id(priority, message_type_id, source_node_id)
    local id = (uint32_t(priority        & 0x1F)   << 24)
             | (uint32_t(message_type_id & 0xFFFF) << 8)
             | (uint32_t(source_node_id  & 0x7F))
    return id | (uint32_t(1) << 31)   -- extended-frame marker
end

-- ---------------------------------------------------------------------------
-- Periodic Sender -- assemble and transmit one complete NotifyState transfer
-- per invocation, advance transfer_id modulo 32, and reschedule after PERIOD_MS.
-- ---------------------------------------------------------------------------
local function update()
    local state         = calculate_current_state()       -- TESTING: cycles through states
    local vehicle_state = state_to_vehicle_state(state)    -- map to NotifyState bitmask
    local payload       = encode_notifystate_payload(vehicle_state)
    local frames        = split_into_frames(payload, transfer_id)
    local can_id        = make_message_id(PRIORITY, NOTIFYSTATE_ID, SOURCE_NODE_ID)

    for i = 1, #frames do
        local frame = CANFrame()
        frame:id(can_id)
        frame:dlc(frames[i].dlc)
        for j = 1, frames[i].dlc do
            frame:data(j - 1, frames[i].data[j])   -- data() is 0-indexed
        end
        if not device:write_frame(frame, WRITE_TIMEOUT_US) then
            gcs:send_text(3, "led_pineCAN_testing: failed to send frame " .. i)
        end
    end

    gcs:send_text(6, "led_pineCAN_testing: sent state " .. state)

    transfer_id = (transfer_id + 1) % 32            -- one increment per whole transfer
    return update, PERIOD_MS
end

return update()
