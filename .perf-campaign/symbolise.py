# Maps profile buckets to function names using the ELF's symbol table.
import bisect, re, subprocess, sys

elf, prof = sys.argv[1], sys.argv[2]
out = subprocess.run(["arm-none-eabi-nm", "-nS", elf], capture_output=True, text=True).stdout
syms = []
for line in out.splitlines():
    parts = line.split()
    if len(parts) == 4:
        addr, size, kind, name = parts
        if kind.lower() in "tw":
            syms.append((int(addr, 16), int(size, 16), name))
    elif len(parts) == 3:
        addr, kind, name = parts
        if kind.lower() in "tw":
            syms.append((int(addr, 16), 0, name))
syms.sort()
starts = [s[0] for s in syms]

def lookup(a):
    i = bisect.bisect_right(starts, a) - 1
    if i < 0:
        return "?"
    addr, size, name = syms[i]
    if size and a >= addr + size:
        return "(gap after %s)" % name
    return name

totals, total = {}, 0
for line in open(prof):
    m = re.match(r"^p ([0-9a-f]{8}) (\d+)$", line.strip())
    if m:
        a, c = int(m.group(1), 16), int(m.group(2))
        # A bucket spans 128 bytes; attribute it to the symbol at its base.
        totals[lookup(a)] = totals.get(lookup(a), 0) + c
        total += c

print("  %-46s %8s %7s" % ("symbol", "samples", "share"))
for name, c in sorted(totals.items(), key=lambda kv: -kv[1])[:26]:
    print("  %-46s %8d %6.1f%%" % (name[:46], c, 100.0 * c / total))
print("\n  attributed samples: %d" % total)
