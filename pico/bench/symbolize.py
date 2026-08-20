#!/usr/bin/env python3
"""Turn a captured profile into a list of function names.

    ./run.py <image>.uf2 --out capture.txt
    ./symbolize.py ../../build-pico/O2/yax86_pico_bench.elf capture.txt

The firmware prints buckets as addresses because carrying a symbol table on
the board would cost more flash than the profiler is worth. Each bucket is
attributed to whichever function contains its base address.

Read the top few entries and check anything below them before acting on it.
The tail is not reliable: a bucket lands on the function that starts before
it, so a small function inlined into a larger one is credited to whatever
symbol happens to precede it.
"""

import bisect
import re
import subprocess
import sys

BUCKET_RE = re.compile(r"^p ([0-9a-f]{8}) (\d+)$")
TOP_N = 26


def load_symbols(elf):
    """Function symbols from the ELF, sorted by address, as (addr, size, name)."""
    out = subprocess.run(
        ["arm-none-eabi-nm", "-nS", elf], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        # nm prints size only for symbols that have one.
        if len(parts) == 4:
            addr, size, kind, name = parts
        elif len(parts) == 3:
            addr, kind, name = parts
            size = "0"
        else:
            continue
        if kind.lower() in "tw":
            syms.append((int(addr, 16), int(size, 16), name))
    syms.sort()
    return syms


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: symbolize.py <elf> <capture>")
    elf, capture = sys.argv[1], sys.argv[2]
    syms = load_symbols(elf)
    starts = [s[0] for s in syms]

    def lookup(address):
        i = bisect.bisect_right(starts, address) - 1
        if i < 0:
            return "?"
        start, size, name = syms[i]
        # Past the end of the symbol it follows, so it belongs to no function
        # this ELF names - padding, or a section the profiler covers but nm
        # does not describe.
        if size and address >= start + size:
            return "(gap after %s)" % name
        return name

    totals, total = {}, 0
    with open(capture) as f:
        for line in f:
            match = BUCKET_RE.match(line.strip())
            if not match:
                continue
            name = lookup(int(match.group(1), 16))
            count = int(match.group(2))
            totals[name] = totals.get(name, 0) + count
            total += count
    if total == 0:
        raise SystemExit("no profile buckets in %s" % capture)

    print("  %-46s %8s %7s" % ("symbol", "samples", "share"))
    for name, count in sorted(totals.items(), key=lambda kv: -kv[1])[:TOP_N]:
        print("  %-46s %8d %6.1f%%" % (name[:46], count, 100.0 * count / total))
    print("\n  attributed samples: %d" % total)


if __name__ == "__main__":
    main()
