#!/usr/bin/env python3
"""Dump every MMIO write in an mmiotrace file as: index offset value.

usage: dump_writes.py TRACE

Addresses are normalised to offsets inside the traced BAR (MAP lines).
Ring BASE registers (0xd4300-0xd45ff, offset & 0xf == 0) are shown as BASE.
Consecutive identical writes are collapsed to one line with a 'xN' count.
"""
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    bases = {}
    rows = []
    with open(sys.argv[1], errors="replace") as fh:
        for line in fh:
            f = line.split()
            if not f:
                continue
            if f[0] == "MAP" and len(f) >= 6:
                bases[int(f[2], 0)] = int(f[3], 16)
                print("MAP id=%s phys=%s size=%s" % (f[1], f[2], f[4]))
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
            rows.append((mid, off, val))

    i = 0
    n = 0
    while i < len(rows):
        j = i
        while j + 1 < len(rows) and rows[j + 1] == rows[i]:
            j += 1
        mid, off, val = rows[i]
        if 0xD4300 <= off < 0xD4600 and (off & 0xF) == 0:
            v = "BASE"
        else:
            v = "%08x" % val
        cnt = j - i + 1
        print("%4d  map%d  %06x %s%s" % (n, mid, off, v,
                                         "  x%d" % cnt if cnt > 1 else ""))
        n += cnt
        i = j + 1
    print("total writes: %d" % len(rows))


if __name__ == "__main__":
    main()
