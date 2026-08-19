# PineCAN LED README

## list of relevant files for the PR

These are the files most relevant when reviewing changes to the lighting
program. The remaining `Core` files are primarily STM32-generated peripheral,
startup, and support code.

- `Core/Src/main.cpp` — Application entry point. Initializes CAN, DMA, and the
  LED timer, services PineCAN, detects vehicle-state changes, selects a lighting
  pattern, and generates/pushes an LED frame every 20 ms.
- `Core/Inc/can_manager.hpp` and `Core/Src/can_manager.cpp` — The
  transport-independent CAN decoding layer. It forwards CAN servicing and
  cached-state reads to `can.c`, detects state changes, and maps the raw
  NotifyState bitmask to a lighting state.
- `Core/Src/can/can.c` (with `Core/Inc/can/can.h`) — CAN peripheral and PineCAN
  integration. It initializes PineCAN, runs its periodic service, decodes
  NotifyState messages in the receive handler, and safely caches the latest raw
  vehicle state for the main loop.
- `Lighting/Src/new_lighting_controller.cpp` (with
  `Lighting/Inc/new_lighting_controller.hpp`) — LED output engine. It owns the
  animation state and LED buffers, expands zones into physical LED colours,
  encodes RGB/RGBW data, and streams it through PWM/DMA.
- `Lighting/Src/new_pattern_table.cpp` (with
  `Lighting/Inc/new_pattern_table.hpp`) — Lighting definitions for each vehicle
  state. This is the main file to edit for colours, brightness, active zones,
  and animations.
- `Lighting/Inc/new_pattern_types.hpp` — Data structures and enums used to
  describe solid, breathing, and strobing zone appearances.
- `Lighting/Inc/conversions.hpp` and `Lighting/Src/conversions.cpp` — Shared
  colour values plus the lighting-state and control-domain enums used by both
  CAN decoding and the LED engine.
- `Lighting/Inc/new_rev4_config.hpp` and `Lighting/Inc/new_rev5_config.hpp` —
  Board-specific LED counts, chip types, zone-to-LED mappings, buffer sizes, and
  timer settings. The build selects the appropriate revision.
- `../../pinecan/common/pinecanCommon.c` and `../../pinecan/CMakeLists.txt` —
  Shared PineCAN fixes identified while debugging this target: the libcanard
  memory pool is explicitly aligned to 8 bytes, and runtime assertions are now
  opt-in rather than enabled automatically for Debug builds.

## Build and flash setup

The lighting target is a CMake project inside the larger `efs-pinecan`
repository. It depends on the shared `pinecan/` and `dsdl/` directories, so keep
the complete repository checked out and initialize its submodules before the
first build:

```powershell
git submodule update --init --recursive
```

The available configure and build presets are:

| Board | Debug | Release |
|---|---|---|
| Revision 4 | `rev4-debug` | `rev4-release` |
| Revision 5 | `rev5-debug` | `rev5-release` |

The configure preset and build preset must always match.

### VS Code

Install these VS Code extensions:

- **CMake Tools** by Microsoft (`ms-vscode.cmake-tools`)
- **STM32CubeIDE for Visual Studio Code** by STMicroelectronics

Either open `targets/efs-can-lighting` directly with **File → Open Folder**, or
keep the monorepo root open and add the following to the root
`.vscode/settings.json`:

```json
{
    "cmake.sourceDirectory": "${workspaceFolder}/targets/efs-can-lighting",
    "cmake.useCMakePresets": "always"
}
```

After opening the workspace:

1. Run **CMake: Select Configure Preset** from the Command Palette and choose the
   required board/build combination, for example `rev5-debug`.
2. Run **CMake: Configure**.
3. Run **CMake: Select Build Preset** and choose the same preset selected in
   step 1, for example `rev5-debug`.
4. Run **CMake: Build**. Use the CMake Tools command, not the STM32 extension's
   Build button.
5. To program the board, run **CMake: Build Target** and select `flash`.

For `rev5-debug`, the build produces:

```text
build/rev5-debug/efs-can-lighting.elf
build/rev5-debug/efs-can-lighting.hex
build/rev5-debug/efs-can-lighting.bin
```

The other presets use the corresponding directory under `build/`.

### Command line

Run CMake from the lighting target directory:

```powershell
cd targets/efs-can-lighting
cmake --preset rev5-debug
cmake --build --preset rev5-debug
```

Replace `rev5-debug` in both commands with the required preset. To build and
flash through ST-LINK, run:

```powershell
cmake --build --preset rev5-debug --target flash
```

Both workflows require CMake, Ninja, and the Arm GNU toolchain
(`arm-none-eabi-gcc`) to be available. Flashing also requires
`STM32_Programmer_CLI` on `PATH`.

This folder contains the data-driven lighting engine for the PineCAN LED board.
It replaces the old per-LED class hierarchy (`led`/`sk6812`/`ws2812`/
`lighting_controller`/`*_control_state_classes`) with three moving parts:

| File | Role |
|------|------|
| `new_lighting_controller.{hpp,cpp}` | The engine. Owns the PWM/DMA output, the animation loop, and the four entry points (`led_init`, `Select_Pattern`, `Generate_Leds`, `Push_Leds`). You rarely touch this. |
| `new_pattern_table.{hpp,cpp}` | **The one file to edit for colours, brightness, and animations.** One row per lighting state, one column per zone. |
| `new_pattern_types.hpp` | The `ZoneAppearance` / `AnimType` / `AnimParams` data types the table is built from. |
| `new_rev4_config.hpp`, `new_rev5_config.hpp` | Per-board facts (LED count, chip types, zone→LED mapping, buffer sizes, timer). One is selected at compile time. |
| `newrev_Config_Template.hpp` | Comment-only template. Copy it to add a new board revision. |
| `conversions.hpp` | Shared colour palette (`RED`, `PURPLE`, …) and the `ControlDomain` / `LightingStateTransition` enums. Also used by `can_manager.hpp`, so treat it as shared. |

## How a frame is produced

1. `main.cpp` polls the CAN cache and calls `interpretVehicleState()` to turn the
   raw `ardupilot.indication.NotifyState` bitmask into a state number (0–8).
2. On a state change, `Select_Pattern(state)` copies that row of
   `pattern_table` into the engine's `current_appearance[]`.
3. Every ~20 ms the inner loop calls `Generate_Leds()` (advances animations,
   expands active zones into a per-LED colour buffer) then `Push_Leds()`
   (encodes the buffer into the DMA-streamed PWM signal).

---

## PineCAN (CAN receive pipeline)

`Core/Src/can.c` is the **single owner** of all PineCAN interaction for this
target, mirroring the efs-pinecan single-servo driver reference. It owns the
`CanardInstance`, the `NodeStatus`, the RX handler body, and the async
`vehicle_state` cache. `CANManager` (see Decode below) owns none of this — it
only forwards to `can.c`.

### Handler registration — `pinecan_handlers.h`

Header-only, no accompanying `.c`. PineCAN's `pinecanCommon.c` includes it and
registers handlers at **compile time** via the `RX_HANDLER_LIST` macro, so no
edits to the PineCAN source tree are needed:

```c
REGISTER_RX_HANDLER(TYPE, HANDLER, TRANSFER_KIND)
//   TYPE          — base name of the DSDL type (TYPE_ID + TYPE_SIGNATURE must exist)
//   HANDLER       — function called when a matching transfer arrives
//   TRANSFER_KIND — REQUEST | RESPONSE | BROADCAST
```

This target registers exactly one: `ARDUPILOT_INDICATION_NOTIFYSTATE` (BROADCAST)
→ `handleNotifyState`.

### Functions (all in `can.c`)

| Function | What it does |
|----------|--------------|
| `initCAN()` | Builds `PinecanInit` from the CubeMX `hcan1`, sets `NodeStatus` defaults, calls `pinecanInit` once. On failure leaves `pinecan_initialized` false so `canService()` never calls into PineCAN. |
| `canService()` | Called every main-loop iteration; gates `pinecan1ms()` to fire once per millisecond (TX-queue drain + stale-transfer cleanup + NodeStatus). No-op until `initCAN()` succeeds. |
| `handleNotifyState()` | Dispatched by PineCAN's `onTransferReceived` on a complete NotifyState broadcast (**interrupt context**). Thin: decode payload → cache `vehicle_state` on success, discard silently on decode failure. No mapping. |
| `canGetLatestVehicleState()` | Parameterless getter polled by the main loop. Returns the last cached raw bitmask, or 0 before any message. No mapping. |

### Async cache

`latest_vehicle_state` is written in interrupt context (`handleNotifyState`) and
read in the main loop (`canGetLatestVehicleState`). It's `volatile` so the poll
read isn't hoisted out of the loop, and because 64-bit access is not atomic on
Cortex-M4, reads/writes are bracketed with a brief IRQ critical section
(`__disable_irq`/`__enable_irq`) to avoid torn values.

---

## Decode (state mapping)

`CANManager` (`can_manager.{hpp,cpp}`) is the pure, transport-agnostic, testable
core. It holds **no** PineCAN or HAL state — `service()` and
`getLatestVehicleState()` are thin forwarders to `can.c`, and the mapping is a
pure function safe to call directly in unit tests.

| Function | What it does |
|----------|--------------|
| `CANManager::service()` | Forwards to `canService()`. |
| `CANManager::getLatestVehicleState()` | Forwards to `canGetLatestVehicleState()`. No mapping. |
| `CANManager::vehicleStateChanged(raw)` | True if `raw` differs from the previous call. First call returns false to establish the baseline, so the initial cached 0 isn't delivered as a spurious change. |
| `CANManager::mapVehicleState(bitmask)` | Pure (no globals/HAL/PineCAN). Maps a 64-bit `vehicle_state` bitmask to a `LightingStateTransition`. |
| `interpretVehicleState(bitmask)` | Free function; thin delegator to `mapVehicleState`. Call from the main loop when `vehicleStateChanged()` returns true. |

Bit mapping and precedence (highest wins) are in the
[Which 6 are actually used](#which-6-are-actually-used) table below.

---

## The 9 patterns (states)

`pattern_table` in `new_pattern_table.cpp` has 9 rows, indexed directly by the
`LightingStateTransition` enum (`conversions.hpp`). Each row lists which of the 8
zones (`ControlDomain`) are lit, their colour, brightness (0–100 **percent**),
and animation.

Colours: **MAIN** is always `PURPLE @ 5%` (a dim "powered" glow). Animations are
`SOLID` unless noted. Only two animated cells exist in the whole table:
GROUND's beacon (breathe) and every active STROBE (double-flash, 800 ms).

| # | State | Active zones (colour @ brightness) | Reachable? |
|---|-------|------------------------------------|:----------:|
| 0 | `GROUND`  | MAIN purple@5 · BEACON red **breathe** (0→50%, 2 s) | ✅ |
| 1 | `STANDBY` | MAIN purple@5 · BEACON red@99 · STROBE orange@99 **strobe** | ✅ |
| 2 | `TAXI`    | MAIN purple@5 · TAXI white@99 · BEACON red@99 · BRAKE orange@99 | ❌ unused |
| 3 | `TAKEOFF` | MAIN purple@5 · BEACON red@99 | ✅ |
| 4 | `FLIGHT`  | MAIN purple@5 · NAV blue@99 · BEACON red@99 · STROBE orange@99 **strobe** | ✅ |
| 5 | `BRAKE`   | MAIN purple@5 · BRAKE orange@99 | ❌ unused |
| 6 | `LANDING` | MAIN purple@5 · LANDING white@99 · NAV blue@99 · BEACON red@99 · STROBE orange@99 **strobe** | ✅ |
| 7 | `SEARCH`  | MAIN purple@5 · SEARCH white@99 | ❌ unused |
| 8 | `STARTUP` | MAIN purple@5 | ✅ |

### Which 6 are actually used

Only **6 of the 9** rows can be reached today, because
`ardupilot.indication.NotifyState` has no bit that maps to taxiing, braking, or
comms-lost. The mapping lives in `CANManager::mapVehicleState` (`can_manager.hpp`):

| NotifyState bit | → State |
|-----------------|---------|
| (bitmask == 0)      | `GROUND` (0) |
| bit 1 `ARMED`       | `STANDBY` (1) |
| bit 23 `IS_TAKING_OFF` | `TAKEOFF` (3) |
| bit 2 `FLYING`      | `FLIGHT` (4) |
| bit 22 `IS_LANDING` | `LANDING` (6) |
| bit 0 `INITIALISING`| `STARTUP` (8) |

Precedence (highest wins): `IS_TAKING_OFF > IS_LANDING > FLYING > ARMED > INITIALISING`.
A non-zero state with no mapped bit returns `UNRECOGNIZED_STATE` (0xFF), which
`Select_Pattern` ignores (holds the previous appearance).

**Used:** GROUND, STANDBY, TAKEOFF, FLIGHT, LANDING, STARTUP.
**Defined but unreachable:** TAXI (2), BRAKE (5), SEARCH (7).

The three unused rows are kept on purpose so `pattern_table[state]` stays a
direct index with no remapping layer. If a future NotifyState (or a different
message) ever exposes taxi/brake/comms-lost bits, add them to
`mapVehicleState` and the rows are already there.

### Editing a pattern

Edit only `new_pattern_table.cpp`. Use the helpers at the top of the file:

- `SOLID(colour, brightness)` — static colour.
- `BREATHE(colour, period_ms, min_pct, max_pct)` — brightness ramps up/down.
- `STROBE(colour, brightness, period_ms)` — double-flash then idle.
- `OFF()` — zone dark. (Always use `OFF()`, never `brightness = 0`.)

Colours come from the palette in `conversions.hpp`. Brightness is a **percentage
(0–100)**, not 0–255.

---

## Board config files

Everything that differs between physical boards lives in a single
`new_revN_config.hpp`. The engine and the pattern table are board-agnostic.

Exactly one config is compiled in, selected by a `REV<N>` preprocessor define.
The switch is in `new_lighting_controller.hpp`:

```cpp
#if defined(REV5)
#include "new_rev5_config.hpp"
#elif defined(REV4)
#include "new_rev4_config.hpp"
#endif
```

`REV4` / `REV5` are **not** set in source — they come from the build
configuration's compiler defines in `.cproject` ("REV4 Debug/Release",
"REV5 Debug/Release"). Pick the matching IDE build configuration for your board.

### Fields in a config file

| Field | Meaning |
|-------|---------|
| `BOARD_REV` | The numeric revision (documentation; matches the `REV<N>` define). |
| `NUM_LEDS` | Count of physically addressable LEDs, in wiring (data-line) order. |
| `led_types[NUM_LEDS]` | Per-LED chip protocol: `CHIP_RGB` (3 ch / 24 bit) or `CHIP_RGBW` (4 ch / 32 bit). |
| `NUM_LEDS_PADDING` / `PADDING_SIZE` | Reset/latch gap prepended to the bank buffer. `PADDING_SIZE = NUM_LEDS_PADDING * 32`. |
| `BANK_OUTPUT_BUFFER_SIZE` | Bytes for one bank = Σ(each LED's bit width: 24 or 32) + `PADDING_SIZE`. Get this wrong and the DMA chain truncates/overruns. |
| `DMA_OUTPUT_BUFFER_SIZE` | `BANK_OUTPUT_BUFFER_SIZE * 2` (double-buffered). |
| `zone_leds[CD_LENGTH]` | **Global** zone→LED bitmask (bit N set = LED N belongs to that zone). One fixed set per board, not per state. Must fit within `NUM_LEDS` bits. |
| `LED_TIM` / `LED_TIM_CHANNEL` | HAL timer handle + channel driving the LED PWM output. |

> ⚠️ Naming caveat: `CHIP_RGB` maps to the legacy class `SK6812` and `CHIP_RGBW`
> to legacy `WS2812` — **inverted from real-world part naming** in this repo.
> Use the `CHIP_*` names in all new code; don't reintroduce the legacy names.

### Current boards

- **rev4** — 10 LEDs, all `CHIP_RGBW`. Zones: corners `{0,2,3,5}`, sides
  `{6,7,8,9}`, E/W `{1,4}`.
- **rev5** — 12 LEDs, mixed (`{0,5,6,11}` are `CHIP_RGB`, the rest `CHIP_RGBW`).
  Three concentric rings of four: SIDE, OUTER, INNER.

---

## Adding a new board revision

A new PCB should require a **new config file only** — no engine, pattern, or
`main.cpp` changes.

1. Copy `newrev_Config_Template.hpp` to `new_rev<N>_config.hpp`.
2. Uncomment and fill in **every** field (include guard, `#include`s,
   `BOARD_REV`, `NUM_LEDS`, `led_types[]`, padding, buffer sizes, `zone_leds[]`,
   `LED_TIM`, `LED_TIM_CHANNEL`). Follow the per-field notes in the template.
3. Double-check `BANK_OUTPUT_BUFFER_SIZE` by hand against `led_types[]`
   (24 bits per `CHIP_RGB`, 32 per `CHIP_RGBW`, plus padding). A copy-pasted
   value from another rev is the most common bug here.
4. Add a branch in `new_lighting_controller.hpp`:

   ```cpp
   #if defined(REV5)
   #include "new_rev5_config.hpp"
   #elif defined(REV4)
   #include "new_rev4_config.hpp"
   #elif defined(REV<N>)
   #include "new_rev<N>_config.hpp"
   #endif
   ```

5. Add a `REV<N>` build configuration in the IDE (define `REV<N>` in that
   config's preprocessor symbols, alongside the existing rev configs in
   `.cproject`).

That's it — the pattern table and engine pick up the new board automatically
because they only reference the names the config provides.
