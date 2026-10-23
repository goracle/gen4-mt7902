#!/usr/bin/env python
# Run as root while mt76 (mt7921e) is bound and idle. Read-only.
# Prints the same non-zero words as [REGDUMP] in dmesg, in vendor address form.
import mmap, os, struct, sys
dev = sys.argv[1] if len(sys.argv) > 1 else "0000:02:00.0"
fd = os.open("/sys/bus/pci/devices/%s/resource0" % dev, os.O_RDONLY)
m = mmap.mmap(fd, 0x200000, mmap.MAP_SHARED, mmap.PROT_READ)
for base, ln in ((0x7c024000, 0x800), (0x7c026000, 0x100), (0x7c027000, 0x100), (0x7c060000, 0x100), (0x74030000, 0x200)):
    for o in range(0, ln, 4):
        bar = 0xd0000 + (base & 0xffff) + o
        v = struct.unpack("<I", m[bar:bar+4])[0]
        if v:
            print("0x%08x=0x%08x" % (base + o, v))
