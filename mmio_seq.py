#!/usr/bin/env python3
"""Decode an mmiotrace file into a chip-address access sequence and diff two traces.

usage:
  mmio_seq.py decode TRACE [--table mt7902.c] [--until-first-cmd] > out.txt
  mmio_seq.py diff   GOOD.txt MINE.txt

decode: parses MAP/W/R lines, maps BAR offsets back to chip addresses using the
        bus2chip table found in mt7902.c, collapses polling reads (same address
        read repeatedly) and prints one line per access.  Writes only with --writes.
diff:   compares the WRITE sequences of two decoded files (address + value),
        ignoring pure polling reads, and prints the first divergence plus the
        writes mt76 makes that the other trace never makes (by address).
"""
import re, sys

def load_table(path):
    src = open(path).read()
    tbl = [tuple(int(x, 16) for x in m.groups()) for m in
           re.finditer(r'\{(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\}', src)]
    return tbl

def to_chip(tbl, off):
    for c, b, r in tbl:
        if b <= off < b + r:
            return c + (off - b)
    return None

def decode(trace, table, writes_only, until_cmd):
    tbl = load_table(table)
    base = None
    remap = 0
    out = []
    prev = None
    for l in open(trace):
        p = l.split()
        if not p:
            continue
        if p[0] == 'MAP' and base is None:
            base = int(p[3], 16)
            continue
        if p[0] not in ('W', 'R') or len(p) < 6 or base is None:
            continue
        off = int(p[4], 16) - base
        val = int(p[5], 16)
        if 0x40000 <= off < 0x50000:
            # mt76 reprograms this window via the remap reg at BAR 0xfe24c (low 16 bits = chip addr high half)
            c = ((remap & 0xffff) << 16) | (off & 0xffff)
            if 0x18000000 <= c < 0x18100000:      # conn_infra alias of 0x7c0xxxxx (seen: 0x18060010 == LPCTL 0x7c060010)
                c += 0x7c000000 - 0x18000000
        else:
            c = to_chip(tbl, off)
        if off == 0xfe24c and p[0] == 'W':
            remap = val
        addr = c if c is not None else 0xB0000000 | off
        key = (p[0], addr)
        if p[0] == 'R':
            if writes_only:
                continue
            if prev == key:
                out[-1][3] = val
                out[-1][4] += 1
                continue
        prev = key
        out.append([p[0], addr, val, val, 1, float(p[2])])
        if until_cmd and p[0] == 'W' and addr == 0x7c0243f8 and val != 0:   # first real command (ring-15 cidx > 0)
            break
    return out

def fmt(e):
    k, a, v, last, n, t = e
    s = f"{t:.6f} {k} 0x{a:08x} 0x{v:x}"
    if n > 1:
        s += f" (x{n}, last 0x{last:x})"
    return s

def read_writes(path):
    res = []
    for l in open(path):
        p = l.split()
        if len(p) >= 4 and p[1] == 'W':
            res.append((int(p[2], 16), int(p[3], 16)))
    return res

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    if sys.argv[1] == 'decode':
        trace = sys.argv[2]
        table = 'mt7902.c'
        if '--table' in sys.argv:
            table = sys.argv[sys.argv.index('--table') + 1]
        ents = decode(trace, table, '--writes' in sys.argv, '--until-first-cmd' in sys.argv)
        for e in ents:
            print(fmt(e))
    elif sys.argv[1] == 'diff':
        good, mine = read_writes(sys.argv[2]), read_writes(sys.argv[3])
        # skip IRQ-handler noise: 0x7c024204 / 0x7c024200 are INT_ENA/INT_STA churn
        noise = {0x7c024204, 0x7c024200, 0x7c00e24c}
        g = [w for w in good if w[0] not in noise]
        m = [w for w in mine if w[0] not in noise]
        i = 0
        while i < min(len(g), len(m)) and g[i] == m[i]:
            i += 1
        print(f"{i} identical leading writes (noise filtered)")
        if i < min(len(g), len(m)):
            print(f"first divergence at write #{i}:")
            print(f"  good: 0x{g[i][0]:08x} <- 0x{g[i][1]:x}")
            print(f"  mine: 0x{m[i][0]:08x} <- 0x{m[i][1]:x}")
        gaddr, maddr = {a for a, _ in g}, {a for a, _ in m}
        print("addresses written by good but never by mine:")
        for a in sorted(gaddr - maddr):
            print(f"  0x{a:08x}")
        print("addresses written by mine but never by good:")
        for a in sorted(maddr - gaddr):
            print(f"  0x{a:08x}")

main()
