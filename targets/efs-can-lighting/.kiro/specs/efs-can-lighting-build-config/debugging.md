# Debugging: LED output dead when flashed via CMake/CLI

## FINAL WORKING CHANGES

**Status: confirmed working on REV5 hardware on 2026-08-18.** The user confirmed
that the CMake `rev5-debug` firmware started working with the complete set of
changes below. Preserve this section and these changes when cleaning up the
temporary diagnostic code.

This is the final working combination. Some items fix independent defects, so do
not remove one merely because another item also affects the same symptom.

### 1. Do not enable the TIM2 update interrupt

`Core/Src/main.cpp` must start TIM2 without its update interrupt:

```cpp
HAL_TIM_Base_Start_IT(&htim6);

// TIM2 is the ~495 kHz LED PWM/DMA carrier. Its update ISR starves -O0 builds.
HAL_TIM_Base_Start(&htim2);
```

Do **not** restore `HAL_TIM_Base_Start_IT(&htim2)`. LED output uses the TIM2 CC1
DMA request enabled by `HAL_TIM_PWM_Start_DMA()`; it does not require UIE or the
TIM2 update ISR.

### 2. Keep the libcanard arena aligned to 8 bytes

`pinecan/common/pinecanCommon.c` must retain:

```c
static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE]
    __attribute__((aligned(8)));
```

libcanard casts this byte arena to structures containing 4- and 8-byte members.
Without explicit alignment, newer GCC code generation can issue `LDRD`/`STRD`
against an odd address and raise an UNALIGNED UsageFault/HardFault on CAN RX.

### 3. PineCAN runtime assertions are opt-in

`pinecan/CMakeLists.txt` must not automatically define `PINECAN_DEBUG` merely
because `CMAKE_BUILD_TYPE` is `Debug`. The final configuration is:

```cmake
option(PINECAN_ENABLE_ASSERTS "Enable PineCAN runtime assertions" OFF)

if(PINECAN_ENABLE_ASSERTS)
    target_compile_definitions(pinecan INTERFACE PINECAN_DEBUG=1)
endif()
```

This makes the default CMake Debug behavior match STM32CubeIDE Debug. Previously,
foreign or rejected CAN frames could trip `PINECAN_ASSERT`, enter newlib's
`__assert_func`, attempt formatted I/O, call `abort`, and remain forever in
`_exit()` from interrupt context. The final working Debug ELF was checked with
`arm-none-eabi-nm`: it contains no `__assert_func`.

Assertions can still be requested deliberately with:

```powershell
cmake --preset rev5-debug -DPINECAN_ENABLE_ASSERTS=ON
```

Do not enable them for the known-working production-like Debug image until the
CAN return-value assertions and embedded assert handler are redesigned.

### 4. VS Code must use the real preset and artifact paths

The tracked target-specific files under `.vscode/` use `rev5-debug`, not the
nonexistent `Debug` preset:

```text
cmake --preset=rev5-debug
cmake --build --preset=rev5-debug
build/rev5-debug/efs-can-lighting.elf
build/rev5-debug/efs-can-lighting.hex
```

The ST-LINK launch configuration also issues GDB `load`. The repository-root
`.gitignore` ignores only `/.vscode/`, allowing
`targets/efs-can-lighting/.vscode/{tasks,launch}.json` to be versioned through
the target's explicit exceptions.

### 5. Build cleanly and program the current HEX under reset

The exact build used for the confirmed working image was:

```powershell
cd targets/efs-can-lighting
cmake --preset rev5-debug
cmake --build --preset rev5-debug --clean-first
```

The resulting image was 31.86 KiB and was successfully programmed and verified
through the connected STLINK-V3MINIE using connect-under-reset:

```powershell
STM32_Programmer_CLI `
  -c port=SWD mode=UR reset=HWrst `
  -w build/rev5-debug/efs-can-lighting.hex `
  -v --hardRst
```

If ST-LINK reports `Unable to get core ID`, stop CubeIDE/debug-server sessions,
power-cycle the board, and retry connect-under-reset. After this programming
method the target may remain halted; power-cycle it once to boot the verified
image normally. That final power cycle was part of the confirmed working test.

### 6. Diagnostic instrumentation is not part of the final firmware

The SRAM2 trace header, event calls, fault counters, and ELF/trace helper scripts
were useful during isolation but were removed from the final working build. Do
not confuse those removals with removal of the three firmware fixes above:

- TIM2 starts without its update interrupt.
- `canardMemPool` remains 8-byte aligned.
- PineCAN assertions remain opt-in and off by default.

### Final verification evidence

- `rev5-debug` configured and linked successfully at `-O0 -g3`.
- The ELF contained no `__assert_func` and placed `canardMemPool` at an aligned
  address (`0x20000CB8` in the confirmed image).
- STM32CubeProgrammer identified the STM32L43xxx/L44xxx, erased sectors 0-15,
  programmed 31.86 KiB at `0x08000000`, and reported
  `Download verified successfully`.
- After the final target power cycle, the user confirmed the Debug firmware was
  working.

---

> ## Historical note: changes outside this target
>
> `pinecan/common/pinecanCommon.c` has been modified on this debug branch:
>
> ```c
> -static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE];
> +static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE] __attribute__((aligned(8)));
> ```
>
> This was initially the only edit outside `targets/efs-can-lighting/`. The final
> working set also changes `pinecan/CMakeLists.txt` to make runtime assertions
> opt-in. Rationale and evidence for the alignment change are in
> [Confirmed root cause 2](#confirmed-root-cause-2-misaligned-canardmempool-hardfaults-on-the-first-can-frame)
> below and in `DEBUG_README.md` section 1.4.
>
> The hardware test succeeded with this alignment fix. It is now part of the
> final working changes and must not be reverted during diagnostic cleanup.

## Symptom

Firmware flashed with `cmake --build --preset=rev5-debug --target flash` produces no LED
output at all. The strip holds whatever colour a previous flash left latched, or stays off
if there was none. The same source flashed from STM32CubeIDE (REV5 Debug and REV5 Release)
drives the LEDs correctly.

Isolated further by hardcoding a fixed colour pattern (no CAN input required): CubeIDE
pushes the pattern, the CMake/CLI build pushes nothing. This removes CAN, DroneCAN decode,
and Mission Planner from the picture — the failure is in the LED PWM/DMA output path or in
what actually reaches the chip.

Relevant hardware/config: STM32L431KC, LED output on **TIM2_CH1 / PA5 / AF1**, driven by
**DMA1_Channel5** (`DMA_REQUEST_4`, memory-to-peripheral, circular, byte-aligned memory,
word-aligned peripheral). `LED_TIM`/`LED_TIM_CHANNEL` are defined in
`Lighting/Inc/new_rev5_config.hpp`.

## Ruled out (with evidence)

Comparison was between `build/rev5-debug/efs-can-lighting.elf` and
`REV5 Debug/efs-can-lighting.elf`.

| Hypothesis | Result |
|---|---|
| CMake build missing source files | Ruled out. The compiled set matches CubeIDE's `objects.list` exactly (same HAL drivers, same CubeMX application sources, same startup file). |
| `led_init` / PWM start miscompiled | Ruled out. `_Z8led_initv` is **instruction-for-instruction identical**, including `HAL_TIM_PWM_Start_DMA(&htim2, TIM_CHANNEL_1, dma_output_buffer, 1344)`. |
| PA5 alternate-function setup missing or GC'd | Ruled out. `HAL_TIM_MspPostInit` is present in both and does not even appear in the codegen diff. |
| Vector table wrong / handlers unlinked | Ruled out. Both tables are 99 entries, initial SP `0x20010000`, identical set of unused slots, and DMA1_Ch5 (IRQ15), CAN1_RX0 (IRQ20), TIM2 (IRQ28), TIM6_DAC (IRQ54) are all populated. |
| C++ name mangling breaking weak HAL callback overrides | Ruled out. `HAL_TIM_PWM_PulseFinishedCallback` and `HAL_TIM_PWM_PulseFinishedHalfCpltCallback` are strong, unmangled `T` symbols in both. |
| DMA buffer alignment | Ruled out. `dma_output_buffer` is 4-byte aligned in both, and the DMA is configured `DMA_MDATAALIGN_BYTE` regardless. |
| Section layout / stack clipped | Ruled out. `.isr_vector`, `.text`, `.data`, `.bss`, `._user_heap_stack` all placed identically; same linker script `STM32L431KCUX_FLASH.ld`. |
| DSDL generation mismatch (root `dsdl/generated` vs `Core/dsdlc_generated`) | Not the cause of the LED symptom. The two generators differ only in wrapper naming (`_x_decode` vs `__x_decode` plus `static inline` shims); `ARDUPILOT_INDICATION_NOTIFYSTATE_ID` (20007) and `_SIGNATURE` (`0x631F2A9C1651FDEC`) are identical. Irrelevant now that the fixed-pattern test removed CAN. |
| Missing `-mthumb` in the CMake toolchain | Benign. `-mcpu=cortex-m4` implies Thumb; ARM-mode code for an M-profile core would fail to assemble. |

The ~109 remaining function-level codegen differences are all ±1 instruction. They are noise
from two different compilers: CubeIDE ships **GNU Tools for STM32 12.3.rel1**, while the
CMake build uses the **Arm GNU Toolchain (mingw-w64-i686-arm-none-eabi)** distribution.

**Conclusion: the two firmware images are functionally equivalent.** This is not a missing
code or misconfigured peripheral problem.

## Confirmed defect: `flash` programs a file it does not depend on

From the generated `build/rev5-debug/build.ninja`:

```
build CMakeFiles/flash: CUSTOM_COMMAND efs-can-lighting.elf
  COMMAND = STM32_Programmer_CLI -c port=SWD -w .../efs-can-lighting.hex -v -rst
```

The edge depends on the **`.elf`** but programs the **`.hex`**. The `.hex` was produced by an
`add_custom_command(TARGET ... POST_BUILD)` with `BYPRODUCTS`, which ties it to the link edge
as a whole rather than making it a first-class output. Ninja keys that edge on its command
hash, so a `.hex` that is stale or missing will not be regenerated, and `flash` never asks
for it.

Observed on disk:

```
rev5-debug     elf 7:56:46 PM    hex 7:54:31 PM   <-- hex 2m15s behind the ELF
rev5-release   elf 7:48:10 PM    hex 7:48:10 PM
```

`add_custom_target(flash ... DEPENDS ${CMAKE_PROJECT_NAME})` is also incorrect usage.
`DEPENDS` on a custom target takes **files**; target-level ordering requires
`add_dependencies()`. Under Ninja a bare target name there yields at most an order-only
dependency, which does not force regeneration when the ELF changes.

Net effect: `--target flash` can program an image that is not the current build, while
still reporting `Download verified successfully` — the programmer verifies the hex against
flash memory, not the hex against your sources.

**Fixed** in `CMakeLists.txt`: `.bin`/`.hex` are now produced by
`add_custom_command(OUTPUT ...)` and `flash` depends on the `.hex` file itself. An `ALL`
target keeps both artifacts building during a normal `cmake --build`.

## Primary suspect

### `PINECAN_DEBUG` assert wedges the MCU in the CAN RX IRQ

After the flash-dependency fix above, a confirmed-current `rev5-debug` image was flashed
(`FLASH: 37040 B` = the 36.17 KB the programmer wrote) and the LED was still dead. With the
artifact-sync problem eliminated, symbol comparison leaves exactly one functional difference
between the image that works and the image that does not:

| image | `__assert_func` / `abort` / `_exit` linked | LED |
|---|---|---|
| CMake `rev5-debug` | **yes** | dead |
| CMake `rev5-release` | no | untested since the sync fix |
| CubeIDE `REV5 Debug` | no | works |

Every failing test so far has used the Debug preset.

`pinecan/CMakeLists.txt` sets `PINECAN_DEBUG=1` for any `Debug` build, which turns
`PINECAN_ASSERT` into a real `assert()` (`pinecan/pinecan.h`). CubeIDE never defines
`PINECAN_DEBUG`, so its asserts always compile to `(void)(x)`.

The fatal one is in `pinecan/common/pinecanCommon.c`:

```c
void handleRxFrame(CanardCANFrame *rxFrame) {
    int16_t retVal = canardHandleRxFrame(data.canard, rxFrame, getUptimeMs() * 1000U);
    PINECAN_ASSERT(CANARD_OK == retVal);
}
```

`canardHandleRxFrame` returns negative codes as normal filtering outcomes, not errors:
`-CANARD_ERROR_RX_WRONG_ADDRESS` (canard.c:423), `-CANARD_ERROR_RX_NOT_WANTED`
(canard.c:451, 466), `-CANARD_ERROR_RX_MISSED_START` (canard.c:469). Only NotifyState and
GetNodeInfo are accepted, so any other frame on the bus trips the assert. The path ends in
`Core/Src/syscalls.c`:

```c
void _exit (int status) { _kill(status, -1); while (1) {} }
```

The MCU wedges permanently, inside the CAN1_RX0 IRQ. Confirmed present in the Debug image
by symbol comparison — `__assert_func`, `abort`, `_exit`, `fiprintf`, `_vfiprintf_r`,
`_malloc_r`, `_impure_data`, `__sf` are all linked into the CMake Debug ELF and **absent**
from the CubeIDE one. This also accounts for the size delta (`.data` 12 → 104, `.bss`
8384 → 8720).

Why this matches the fixed-colour test: `main()` calls `initCAN()` — which enables
`CAN_IT_RX_FIFO0_MSG_PENDING` and `HAL_CAN_Start` (pinecanBoard.c:131-136) — **before**
`led_init()` starts the PWM/DMA. With traffic on the bus, an unwanted frame can arrive in
that window, wedge the CPU in the IRQ, and the LED pipeline never starts at all. That is
exactly "holds the previous colour, or stays off if there was none", and it is independent
of whether the pattern is hardcoded.

This was previously dismissed because "rev5-release also fails". That test was invalid:
at the time it was run, `build/rev5-release/efs-can-lighting.elf` was timestamped
**6:59:57 PM** — older than the debug ELF (7:14) and predating the fixed-colour edit. A
stale binary was flashed.

### Discriminating tests

- Flash `rev5-release` (assert compiled out). If the LED works, confirmed.
- Or unplug the CAN bus and reset a `rev5-debug` image. No traffic means no unwanted frame,
  so nothing trips the assert. If the LED lights up with CAN disconnected but not with it
  connected, that is conclusive.

### Suggested fix

Drop the assert on `canardHandleRxFrame`'s return value entirely, or only flag genuine
faults such as `CANARD_ERROR_OUT_OF_MEMORY` / `CANARD_ERROR_INTERNAL`. Separately, stop
coupling `PINECAN_DEBUG` to `CMAKE_BUILD_TYPE` — `Debug` should mean "-O0 with symbols", not
"halt the board on the first foreign CAN frame". Asserting from an IRQ with newlib stdio
underneath is a bad shape regardless.

## Confirmed root cause 2: misaligned `canardMemPool` HardFaults on the first CAN frame

Fixing TIM2 (below) was necessary but not sufficient. With the CPU no longer starved, the
`rev5-release` trace showed a completely healthy main loop that then died:

```
last event: HARDFAULT @ 4937 ms
CFSR  : 0x01000000     -> bit 24, UNALIGNED UsageFault
HFSR  : 0x40000000     -> bit 30, FORCED (escalated to HardFault)
stack : 0x20000A80 0x00000001 0x2000FF54 0x20000AB9 ...
                                         ^^^^^^^^^^ canardMemPool, in a register

SYSTICK      4,937     TIM2_IRQ        0
INNER_LOOP 1,716,462   GENERATE  246   PUSH  246
CAN_RX_IRQ       1     NOTIFY_OK   0   NOTIFY_FAIL 0
```

246 pushes x 20 ms = 4,920 ms, so the LED pipeline ran correctly right up to the fault.
`CAN_RX_IRQ 1` with both NotifyState counters at zero, and no `NOTIFY_RX` event, means the
first frame entered `canardHandleRxFrame()` and never returned.

### Mechanism

`pinecan/common/pinecanCommon.c` declares the arena as a plain byte array:

```c
static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE];
```

`uint8_t` has a required alignment of 1, so the linker may place it on any byte.
`initPoolAllocator()` (canard.c:1891) takes the pointer verbatim — `CanardPoolAllocatorBlock
*abuf = buf;`, no rounding — and blocks are then cast to `CanardRxState*` (canard.c:1521),
`CanardTxQueueItem*` (canard.c:1340) and `CanardBufferBlock*` (canard.c:1649), all of which
contain 4- and 8-byte members. This is undefined behaviour: the compiler may assume those
struct pointers are aligned and emit `LDRD`/`STRD`, which fault on any non-word-aligned
address regardless of `CCR.UNALIGN_TRP`.

Observed addresses (`arm-none-eabi-nm <elf> | Select-String canardMemPool`):

```
CMake   rev5-release   0x20000ab9   ODD             -> HardFault
CMake   rev5-debug     0x20000b14   4-byte aligned  -> no fault
CubeIDE REV5 Release   0x20000ac9   ODD             -> works
CubeIDE REV5 Debug     0x20000acc   4-byte aligned  -> works
CubeIDE REV4 Release   0x20000a5d   ODD             -> works
```

### Why CubeIDE survives it

Misalignment is **not** the discriminator — both Release builds land on an odd address.
The discriminator is the compiler:

| build | toolchain | codegen at `-Os` | result |
|---|---|---|---|
| CMake | Arm GNU Toolchain **15.3.1** (from `.map` library paths) | emits `LDRD`/`STRD` and wider merged accesses | faults |
| CubeIDE | GNU Tools for STM32 **12.3.rel1** | narrower single-word accesses | tolerated |

Unaligned *single-word* `LDR`/`STR` are permitted on Cortex-M4 unless `UNALIGN_TRP` is set;
`LDRD`/`STRD`/`LDM`/`STM` are never permitted. GCC 15 chose the latter, GCC 12 did not. The
CubeIDE builds are therefore running on compiler luck, and a toolchain bump would break
them the same way. `single_servo_driver` shares the declaration and is equally exposed.

### Fix

```c
static uint8_t canardMemPool[CANARD_MEM_POOL_SIZE] __attribute__((aligned(8)));
```

Applied on this branch. See the warning at the top of this file — this is the only change
outside `targets/efs-can-lighting/`, and it is the one line to revert if the fix does not
hold. The proper long-term fix is arguably in libcanard: `initPoolAllocator()` should round
the supplied arena up to `alignof(max_align_t)` rather than trusting the caller.

### Defect in the fault capture, found while diagnosing this

`dbgTraceFault()` snapshots `fault_stack[]` from its own frame, after
`HardFault_Handler` has pushed `{r3, lr}` and `dbgTraceFault` has pushed 8 registers. The
real exception frame is at `MSP + 40`, so the reported 8 words are callee-saved registers,
not `R0-R3/R12/LR/PC/xPSR`. The apparent PC of `0x08002526` decodes to
`HardFault_Handler+0x8`, the handler's own spin loop. `CFSR`/`HFSR` are unaffected and were
sufficient here. Offsets cannot be hardcoded because `-O0` and `-Os` push different
register sets; a `naked` handler passing the frame pointer explicitly is the correct fix.
Not yet done — see `DEBUG_README.md` section 1.5.

## CONFIRMED ROOT CAUSE: TIM2 update ISR starves the CPU

The SRAM2 trace settled this. `main()` stops on the line immediately after
`HAL_TIM_Base_Start_IT(&htim2)`:

```
last event: TIM2_START      (NODE_ID_DONE, one line later, never appears)

                 read 1      read 2     delta
TIM6_IRQ (1 Hz)      33          44        11   -> 11 seconds of real time
SYSTICK  (1 kHz)     10          13         3   -> should have been ~11000
TIM2_IRQ      7,934,182  10,837,859  2,903,677  -> ~264 kHz serviced
DMA_HALF / DMA_FULL   0           0         0
INNER_LOOP            0           0         0
```

TIM2 has `Period = 96` at 48 MHz, so its update event fires every ~2.02 us (~495 kHz).
`HAL_TIM_IRQHandler` is 214 instructions at `-O0` and cannot finish inside that window,
so the core does nothing but re-enter the handler. `SYSTICK` advancing 3 times in 11
seconds shows `HAL_GetTick()` frozen near 13 ms; TIM2 being serviced at ~264 kHz against
a ~495 kHz request shows the handler overrunning its own period.

Fixed in `Core/Src/main.cpp` by starting the timebase without UIE:

```c
-  HAL_TIM_Base_Start_IT(&htim2);
+  HAL_TIM_Base_Start(&htim2);
```

The LED DMA is fed by the TIM2 CC1 DMA request (`CC1DE`), not the update interrupt, so
UIE was never needed. `HAL_TIM_PWM_Start_DMA()` enables the counter itself.

This also explains the other two observations:

- **CMake Release (`-Os`)**: handler is 130 instructions and fits just inside 2.02 us, so
  a sliver of CPU remains — enough to reach `led_init()` and start the DMA, which then
  runs in hardware. Hence "fixed colour works, full program does not".
- **CubeIDE**: the reference ELF calls `HAL_TIM_Base_Start_IT` twice as well, so the
  source is identical. It survived on the same knife edge.

### Correction to the section below

The analysis that follows concluded TIM2 was "not the cause" because
`HAL_TIM_IRQHandler` is 214 instructions in both Debug images. That inference was wrong.
Equal ISR cost does not imply equal outcome when the deciding factor is ISR duration
versus timer period; `-O0` and `-Os` land on opposite sides of that threshold. Retained
below for the instruction-count measurements, which are still accurate.

## Superseded: `-O0` versus `-Os` and the TIM2 update ISR

`Core/Src/main.cpp` calls:

```cpp
HAL_TIM_Base_Start_IT(&htim2);   // TIM2: Period = 96 @ 48 MHz MSI
```

This enables the TIM2 update interrupt on the *same timer generating the WS28xx carrier* —
roughly 500 kHz, one interrupt every ~97 core cycles, which `HAL_TIM_IRQHandler` cannot
complete in. It looked like a promising threshold effect, since the `.cproject` specifies
`-Os` for one C compiler configuration while the CMake `Debug` preset uses `-O0`.

**Not the cause.** Instruction counts for `HAL_TIM_IRQHandler`:

| image | instructions |
|---|---|
| CMake `rev5-debug` | 214 |
| CMake `rev5-release` | 130 |
| CubeIDE `REV5 Debug` | **214** |

CubeIDE's Debug configuration compiles the HAL at `-O0` as well, so the ISR costs exactly
the same in the build that works. Interrupt load cannot be what distinguishes them.

This is still a genuine latent design problem worth fixing on its own — TIM2 is the PWM
carrier, not a periodic tick source, and `HAL_TIM_Base_Start_IT(&htim2)` looks unintended —
but it is not the reason the CMake build is dark.

## Other divergences noted (no known impact)

- `REV5` is defined for C, C++ and ASM by CMake; CubeIDE defines it for C++ only. Currently
  harmless — `REV4`/`REV5` are consumed only in `new_lighting_controller.hpp`.
- CubeIDE uses `-std=gnu++14`, CMake uses `-std=gnu++17`.
- CubeIDE passes `-fno-use-cxa-atexit`; CMake does not. No static objects with non-trivial
  destructors exist, so nothing is affected.
- CubeIDE has `Demos/Inc` on the include path; CMake does not. Nothing currently includes
  from it.
- CubeIDE links `-Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group -static`;
  CMake links `-lm` only and relies on the `g++` driver defaults.

## Useful commands

Comparing two ELFs (run from `targets/efs-can-lighting`):

```powershell
# section layout and sizes
arm-none-eabi-size.exe  "REV5 Debug/efs-can-lighting.elf" "build/rev5-debug/efs-can-lighting.elf"
arm-none-eabi-objdump.exe -h <elf>

# which definition won for a weak/overridable symbol
arm-none-eabi-nm.exe <elf> | Select-String "PulseFinished|MspPostInit"

# symbols present in one image but not the other reveals unexpected library pull-in
arm-none-eabi-nm.exe --defined-only <elf>

# per-function machine-code comparison (normalise addresses before diffing)
arm-none-eabi-objdump.exe -d --no-show-raw-insn <elf>

# vector table contents
arm-none-eabi-objcopy --dump-section .isr_vector=vec.bin <elf>
```

Checking that the artifact about to be flashed is current:

```powershell
Get-ChildItem build\rev5-debug\efs-can-lighting.* | Select-Object Name,LastWriteTime
```

Inspecting what the flash edge really depends on:

```powershell
Select-String -Path build\rev5-debug\build.ninja -Pattern "^build CMakeFiles/flash"
```
