#!/usr/bin/env python3
"""
Read and decode the efs-can-lighting SRAM2 trace buffer over SWD.

The MCU is attached in HOTPLUG mode so it is NOT reset -- a board wedged inside an
interrupt handler or an assert loop can be inspected exactly as it died.

Usage:
    cmake --build --preset=rev5-debug --target trace
    python tools/read_trace.py --elf build/rev5-debug/efs-can-lighting.elf

Layout must stay in sync with Core/Inc/debug_trace.h.
"""

import argparse
import re
import struct
import subprocess
import sys

TRACE_BASE = 0x10000000
MAGIC = 0x4C494754  # 'LIGT'
CAPACITY = 128
COUNTERS = 16
RECORD_FMT = "<IHHII"          # tick, id, seq, a, b
RECORD_SIZE = struct.calcsize(RECORD_FMT)
HEADER_WORDS = 6               # magic, version, boot_count, write_index, capacity, last_event
HEADER_SIZE = HEADER_WORDS * 4 + COUNTERS * 4 + 8 * 4 + 8 * 4
TOTAL_SIZE = HEADER_SIZE + CAPACITY * RECORD_SIZE

EVENTS = [
    "NONE", "BOOT", "HAL_INIT_DONE", "CLOCK_CONFIG_DONE", "GPIO_INIT_DONE",
    "DMA_INIT_DONE", "CAN1_INIT_DONE", "TIM1_INIT_DONE", "TIM6_INIT_DONE",
    "TIM7_INIT_DONE", "TIM2_INIT_DONE", "TIM6_START", "TIM2_START",
    "NODE_ID_DONE", "INITCAN_ENTER", "INITCAN_RESULT", "LED_INIT_ENTER",
    "PWM_DMA_RESULT", "LED_INIT_DONE", "MAIN_LOOP_ENTER", "FIRST_GENERATE",
    "FIRST_PUSH", "SELECT_PATTERN", "STATE_CHANGED", "NOTIFY_RX",
    "NOTIFY_STATE", "HEARTBEAT", "ERROR_HANDLER", "ASSERT", "NMI",
    "HARDFAULT", "MEMMANAGE", "BUSFAULT", "USAGEFAULT",
]

COUNTER_NAMES = [
    "SYSTICK", "DMA_IRQ", "DMA_HALF", "DMA_FULL", "TIM1_IRQ", "TIM2_IRQ",
    "TIM6_IRQ", "TIM7_IRQ", "CAN_RX_IRQ", "NOTIFY_OK", "NOTIFY_FAIL",
    "INNER_LOOP", "GENERATE", "PUSH", "SELECT", "SERVICE",
]

HAL_STATUS = {0: "HAL_OK", 1: "HAL_ERROR", 2: "HAL_BUSY", 3: "HAL_TIMEOUT"}

# Events whose 'a' field is a HAL_StatusTypeDef / PineCAN_Status
STATUS_EVENTS = {"TIM6_START", "TIM2_START", "INITCAN_RESULT", "PWM_DMA_RESULT"}


def read_target(size):
    """Attach without reset and read `size` bytes from the trace buffer."""
    words = (size + 3) // 4
    cmd = [
        "STM32_Programmer_CLI",
        "-c", "port=SWD", "mode=HOTPLUG",
        "-r32", hex(TRACE_BASE), hex(words * 4),
    ]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except FileNotFoundError:
        sys.exit("STM32_Programmer_CLI not found on PATH.")
    except subprocess.TimeoutExpired:
        sys.exit("STM32_Programmer_CLI timed out.")

    text = out.stdout + out.stderr
    if "Error" in text and "0x" not in text:
        sys.exit("Failed to read target:\n" + text)

    # Lines look like:  0x10000000 : 4C494754 00000001 00000003 0000002A
    data = bytearray()
    for line in text.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]+\s*:\s*(.*)$", line)
        if not m:
            continue
        for tok in m.group(1).split():
            if re.fullmatch(r"[0-9A-Fa-f]{8}", tok):
                data += struct.pack("<I", int(tok, 16))

    if len(data) < size:
        sys.exit(
            "Only recovered %d of %d bytes. Raw output:\n%s" % (len(data), size, text)
        )
    return bytes(data[:size])


def load_rodata(elf):
    """Address -> byte map for the read-only sections, so recorded char* pointers can
    be resolved to text. Strings are walked on demand rather than pre-split, because
    the section contains many NUL-separated literals."""
    blob = {}
    if not elf:
        return blob

    # String literals can land in .rodata or in a .rodata.str* subsection that objdump
    # reports separately, so dump every read-only section we might care about.
    for section in (".rodata", ".text", ".ARM.extab"):
        try:
            out = subprocess.run(
                ["arm-none-eabi-objdump", "-s", "-j", section, elf],
                capture_output=True, text=True, timeout=60,
            ).stdout
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue

        for line in out.splitlines():
            # " 8008740 6b696e67 2f70696e 65636100 00000000  king/pineca...."
            m = re.match(r"^\s*([0-9a-f]{4,})\s+((?:[0-9a-f]{2,8}\s+){1,4})", line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            try:
                raw = bytes.fromhex("".join(m.group(2).split()))
            except ValueError:
                continue
            for i, byte in enumerate(raw):
                blob[addr + i] = byte
    return blob


def fmt_str(blob, ptr, limit=160):
    """Read a NUL-terminated string at ptr out of the byte map."""
    if ptr not in blob:
        return "0x%08X" % ptr
    chars = []
    cur = ptr
    while cur in blob and blob[cur] != 0 and len(chars) < limit:
        byte = blob[cur]
        if byte < 0x20 or byte > 0x7E:
            break
        chars.append(chr(byte))
        cur += 1
    if not chars:
        return "0x%08X" % ptr
    return '"%s"  (0x%08X)' % ("".join(chars), ptr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", help="ELF used to resolve string pointers")
    args = ap.parse_args()

    raw = read_target(TOTAL_SIZE)
    off = 0
    magic, version, boot_count, write_index, capacity, last_event = struct.unpack_from(
        "<6I", raw, off
    )
    off += HEADER_WORDS * 4
    counters = struct.unpack_from("<%dI" % COUNTERS, raw, off)
    off += COUNTERS * 4
    fault = struct.unpack_from("<8I", raw, off)
    off += 8 * 4
    fault_stack = struct.unpack_from("<8I", raw, off)
    off += 8 * 4

    if magic != MAGIC:
        print("Trace buffer magic is 0x%08X, expected 0x%08X." % (magic, MAGIC))
        print("Either the firmware was not built with LIGHTING_TRACE=1, or it never")
        print("reached DBG_INIT() in main().")
        return 1

    rodata = load_rodata(args.elf)

    print("=" * 72)
    print("trace buffer @ 0x%08X   version %d   boot #%d" % (TRACE_BASE, version, boot_count))
    print("records written: %d   last event: %s" % (write_index, ev_name(last_event)))
    print("=" * 72)

    print("\nCOUNTERS")
    width = max(len(n) for n in COUNTER_NAMES)
    for name, value in zip(COUNTER_NAMES, counters):
        flag = ""
        if name in ("DMA_HALF", "DMA_FULL") and value == 0:
            flag = "   <-- DMA stream is NOT running"
        if name == "INNER_LOOP" and value == 0:
            flag = "   <-- main loop never iterated"
        print("  %-*s %12d%s" % (width, name, value, flag))

    if fault[0] != 0:
        print("\nFAULT / ASSERT LATCHED")
        if fault[0] == ev_id("ASSERT"):
            print("  __assert_func was called (this is the pinecan PINECAN_ASSERT path)")
            print("  file : %s" % fmt_str(rodata, fault[1]))
            print("  line : %d" % fault[2])
            print("  func : %s" % fmt_str(rodata, fault[3]))
            print("  expr : %s" % fmt_str(rodata, fault[4]))
        else:
            print("  event : %s" % ev_name(fault[0]))
            print("  CFSR  : 0x%08X    HFSR : 0x%08X" % (fault[1], fault[2]))
            print("  BFAR  : 0x%08X    MMFAR: 0x%08X" % (fault[3], fault[4]))
            print("  MSP   : 0x%08X    DFSR : 0x%08X" % (fault[5], fault[6]))
            print("  SHCSR : 0x%08X" % fault[7])
            print("  stack : " + " ".join("0x%08X" % w for w in fault_stack))

    print("\nEVENT LOG (oldest first)")
    total = write_index
    start = 0 if total <= CAPACITY else total - CAPACITY
    if total > CAPACITY:
        print("  (ring wrapped; showing most recent %d of %d)" % (CAPACITY, total))
    print("  %-6s %-10s %-22s %-12s %s" % ("seq", "tick(ms)", "event", "a", "b"))
    for i in range(start, total):
        rec = struct.unpack_from(RECORD_FMT, raw, off + (i % CAPACITY) * RECORD_SIZE)
        tick, ev, seq, a, b = rec
        name = ev_name(ev)
        a_txt = "0x%08X" % a
        if name in STATUS_EVENTS:
            a_txt = HAL_STATUS.get(a, a_txt)
        elif name == "SELECT_PATTERN":
            a_txt = "0xFF (UNRECOGNIZED)" if a == 0xFF else str(a)
        elif name in ("CLOCK_CONFIG_DONE", "HEARTBEAT", "FIRST_GENERATE", "FIRST_PUSH"):
            a_txt = str(a)
        elif name == "ASSERT":
            a_txt = "line %d" % a
        print("  %-6d %-10d %-22s %-12s 0x%08X" % (seq, tick, name, a_txt, b))

    print("\nINTERPRETATION HINTS")
    print("  last event LED_INIT_ENTER but no PWM_DMA_RESULT -> hung inside led_init()")
    print("  PWM_DMA_RESULT != HAL_OK                        -> DMA never started")
    print("  DMA_HALF/DMA_FULL == 0 after LED_INIT_DONE      -> stream not circulating")
    print("  TIM2_IRQ >> SYSTICK                             -> CPU starved by TIM2 ISR")
    print("  CAN_RX_IRQ > 0 but NOTIFY_OK+NOTIFY_FAIL == 0   -> lost in pinecan RX path")
    print("  ASSERT latched                                  -> PINECAN_ASSERT tripped")
    return 0


def ev_name(i):
    return EVENTS[i] if 0 <= i < len(EVENTS) else "EV_%d" % i


def ev_id(name):
    return EVENTS.index(name)


if __name__ == "__main__":
    sys.exit(main())
