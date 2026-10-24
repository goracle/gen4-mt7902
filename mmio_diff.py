#!/usr/bin/env python3
"""Compare the MMIO *write* sequence of two mmiotrace files.

usage: mmio_diff.py GOOD_TRACE OTHER_TRACE [CONTEXT]

* Addresses are normalised to offsets inside the traced BAR (MAP lines), so
  the two boots may have different physical BAR bases.
* Each trace is cut to the window that starts at its first write to WFDMA0
  GLO_CFG (BAR offset 0xd4208) and ends at the first GLO_CFG write that sets
  both DMA-enable bits (bits 0 and 2), i.e. the DMA bring-up.
* Values of ring BASE registers (0xd4300-0xd45ff, offset & 0xf == 0) are DMA
  addresses and are hidden; every other write is compared with its value.
* Output is a unified diff: '-' lines are only in GOOD, '+' lines only in OTHER.
"""
import sys
import os
import time
import difflib

ANCHOR = 0xD4208


def load(path):
    bases = {}
    ops = []
    started = False
    with open(path, errors="replace") as fh:
        for line in fh:
            f = line.split()
            if not f:
                continue
            if f[0] == "MAP" and len(f) >= 6:
                bases[int(f[2], 0)] = int(f[3], 16)
                continue
            if f[0] != "W" or len(f) < 6:
                continue
            try:
                mid = int(f[3])
                addr = int(f[4], 16)
                val = int(f[5], 16)
            except ValueError:
                continue
            off = addr - bases[mid] if mid in bases else addr & 0xFFFFFF
            if off == ANCHOR:
                started = True
            if not started:
                continue
            ops.append((off, val))
            if off == ANCHOR and (val & 0x5) == 0x5:
                break
    return ops


def fmt(off, val):
    if 0xD4300 <= off < 0xD4600 and (off & 0xF) == 0:
        return "%06x BASE" % off
    return "%06x %08x" % (off, val)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    good = [fmt(*o) for o in load(sys.argv[1])]
    other = [fmt(*o) for o in load(sys.argv[2])]
    if not good or not other:
        sys.exit("no write to offset 0x%x found" % ANCHOR)
    for tag, path in (("good", sys.argv[1]), ("other", sys.argv[2])):
        print("%s: %s  (modified %s)" % (tag, path, time.strftime(
            "%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(path)))))
    print("window sizes: good=%d writes, other=%d writes" % (len(good), len(other)))
    for line in difflib.unified_diff(good, other, "good", "other",
                                     n=ctx, lineterm=""):
        print(line)


if __name__ == "__main__":
    main()
