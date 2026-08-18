#!/usr/bin/env python3
"""
ELF diagnostics for efs-can-lighting. Backs the symcheck / listing / compare-ref
CMake targets. See DEBUG_README.md.

Subcommands:
    symcheck <elf> [--map <map>]   report unwanted libc pull-in and who owns __assert_func
    listing  <elf> --out <file>    write a disassembly listing
    compare  <elf> <ref-elf>       per-function machine-code diff, addresses normalised
"""

import argparse
import collections
import os
import re
import subprocess
import sys

OBJDUMP = "arm-none-eabi-objdump"
NM = "arm-none-eabi-nm"

# Symbols that should NOT be in a bare-metal LED controller. Their presence means
# something dragged in newlib stdio/heap -- most likely a live assert().
UNWANTED = [
    ("__assert_func", "assert() is live; a failed assert will hang the MCU"),
    ("abort", "abort() path linked (reached via assert)"),
    ("_exit", "_exit() linked; ends in a bare while(1)"),
    ("fiprintf", "stdio linked"),
    ("_vfiprintf_r", "stdio formatting linked"),
    ("_malloc_r", "heap allocator linked"),
    ("_sbrk", "heap growth linked"),
    ("_impure_data", "newlib reentrancy struct linked"),
]


def run(cmd):
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    except FileNotFoundError:
        sys.exit("Not found on PATH: %s" % cmd[0])
    except subprocess.TimeoutExpired:
        sys.exit("Timed out: %s" % " ".join(cmd))
    return res.stdout


def defined_symbols(elf):
    out = run([NM, "--defined-only", elf])
    syms = {}
    for line in out.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3:
            syms[parts[2].strip()] = (parts[0], parts[1])
    return syms


def cmd_symcheck(args):
    syms = defined_symbols(args.elf)
    print("=" * 72)
    print("symcheck: %s" % os.path.basename(args.elf))
    print("=" * 72)

    hits = [(n, why) for n, why in UNWANTED if n in syms]
    if not hits:
        print("\nClean: no newlib assert/stdio/heap symbols linked.")
    else:
        print("\nFound %d unwanted symbol(s):" % len(hits))
        for name, why in hits:
            addr, kind = syms[name]
            print("  %-16s 0x%s  %s" % (name, addr, why))

    # Who actually provides __assert_func? If it is a libc archive member rather than
    # one of our objects, a tripped assert vanishes into newlib and hangs silently.
    if args.map and os.path.exists(args.map):
        owner = None
        with open(args.map, errors="ignore") as fh:
            lines = fh.readlines()
        for i, line in enumerate(lines):
            if ".text.__assert_func" in line:
                for follow in lines[i:i + 3]:
                    m = re.search(r"0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\S.*)$", follow)
                    if m:
                        owner = m.group(1).strip()
                        break
                if owner:
                    break
        print("\n__assert_func provided by:")
        if owner is None:
            print("  (not found in map)")
        else:
            print("  %s" % owner)
            if "lib" in owner and ".a(" in owner:
                print("  ^ this is the LIBC version -- a failed assert will hang invisibly.")
            else:
                print("  ^ local override is in effect; asserts are captured in the trace.")
    return 0


def cmd_listing(args):
    text = run([OBJDUMP, "-d", "-S", "--no-show-raw-insn", args.elf])
    with open(args.out, "w", encoding="utf-8") as fh:
        fh.write(text)
    print("Wrote %s (%d lines)" % (args.out, text.count("\n")))
    return 0


def parse_functions(elf):
    """Map function name -> normalised instruction list (addresses removed)."""
    text = run([OBJDUMP, "-d", "--no-show-raw-insn", elf])
    funcs = collections.OrderedDict()
    cur = None
    for line in text.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:\s*$", line)
        if m:
            cur = m.group(1)
            funcs[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s*[0-9a-f]+:\s+(.*?)\s*$", line)
        if not m:
            continue
        ins = m.group(1)
        ins = re.sub(r"@.*$", "", ins)                    # trailing comments
        ins = re.sub(r"0x[0-9a-f]+", "A", ins)            # hex literals / addresses
        ins = re.sub(r"\b[0-9a-f]{6,8}\b", "A", ins)      # bare addresses
        ins = re.sub(r"<([^>+]+)(\+[^>]*)?>", r"<\1>", ins)  # keep symbol, drop offset
        funcs[cur].append(re.sub(r"\s+", " ", ins).strip())
    return funcs


def cmd_compare(args):
    if not os.path.exists(args.ref):
        # A missing reference is expected -- STM32CubeIDE cleans its output folders.
        # Report it without failing the build; this is a diagnostic, not a gate.
        print("Reference ELF not found: %s" % args.ref)
        print("Build the reference in STM32CubeIDE, or point at one explicitly:")
        print("  cmake --preset=<preset> -DPINECAN_REF_ELF=<path-to-reference.elf>")
        return 0

    a = parse_functions(args.elf)
    b = parse_functions(args.ref)

    print("=" * 72)
    print("A (this build) : %s  (%d functions)" % (args.elf, len(a)))
    print("B (reference)  : %s  (%d functions)" % (args.ref, len(b)))
    print("=" * 72)

    only_a = [k for k in a if k not in b]
    only_b = [k for k in b if k not in a]
    common = [k for k in a if k in b]
    differ = [k for k in common if a[k] != b[k]]

    print("\nONLY IN A (%d) -- extra code pulled into this build" % len(only_a))
    for k in only_a:
        print("  %s" % k)

    print("\nONLY IN B (%d) -- present in the reference but missing here" % len(only_b))
    for k in only_b:
        print("  %s" % k)

    print("\nDIFFERING BODIES (%d of %d common)" % (len(differ), len(common)))
    print("  NOTE: a +/-1 instruction delta is usually just a different GCC version.")
    print("  Look for large deltas, which indicate a real semantic difference.")
    big = []
    for k in differ:
        delta = len(a[k]) - len(b[k])
        line = "  %-52s A=%-5d B=%-5d delta=%+d" % (k, len(a[k]), len(b[k]), delta)
        if abs(delta) > 4:
            big.append(line)
        print(line)

    if big:
        print("\nLARGE DELTAS (>4 instructions) -- inspect these first")
        for line in big:
            print(line)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("symcheck")
    p.add_argument("elf")
    p.add_argument("--map")
    p.set_defaults(func=cmd_symcheck)

    p = sub.add_parser("listing")
    p.add_argument("elf")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_listing)

    p = sub.add_parser("compare")
    p.add_argument("elf")
    p.add_argument("ref")
    p.set_defaults(func=cmd_compare)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
