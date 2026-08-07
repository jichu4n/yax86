#!/bin/bash
# Download opcode tests from the 8088 hardware-generated CPU test suite.
#
#   https://github.com/SingleStepTests/8088
#
# Each opcode is a separate file, so tests are fetched on demand rather than
# cloning the whole suite. Pass opcodes as hex, with an optional ModRM REG
# extension for group opcodes:
#
#   ./tools/download-cpu-tests.sh 8D 00 80.0
#
# With no arguments, refreshes whatever has already been downloaded.

set -e

readonly BASE_URL="https://raw.githubusercontent.com/SingleStepTests/8088/main"
readonly DEST=".cache/8088-tests"

cd "$(dirname "$0")"/..
mkdir -p "${DEST}"

# The metadata lists which opcodes are undocumented and, for each, a mask of
# the flags the 8088 leaves undefined. Tests are compared with those bits
# cleared, so that emulating undefined behavior is not required.
if [ ! -f "${DEST}/metadata.json" ]; then
    echo "Downloading metadata.json..."
    curl -fsSL "${BASE_URL}/v2/metadata.json" -o "${DEST}/metadata.json"
fi

opcodes=("$@")
if [ ${#opcodes[@]} -eq 0 ]; then
    for f in "${DEST}"/*.MOO; do
        [ -e "${f}" ] || continue
        opcodes+=("$(basename "${f}" .MOO)")
    done
    if [ ${#opcodes[@]} -eq 0 ]; then
        echo "No opcodes given and none downloaded yet. Try: $0 8D"
        exit 1
    fi
fi

failed=()
for opcode in "${opcodes[@]}"; do
    name="${opcode^^}"
    echo "Downloading opcode ${name}..."
    # Group opcodes are split per ModRM REG value, as in "80.0" - a bare "80"
    # has no file of its own. Keep going so one bad name does not abort a batch.
    if curl -fsSL "${BASE_URL}/v2_binary/${name}.MOO.gz" \
            -o "${DEST}/${name}.MOO.gz"; then
        gunzip -f "${DEST}/${name}.MOO.gz"
    else
        rm -f "${DEST}/${name}.MOO.gz"
        failed+=("${name}")
    fi
done

if [ ${#failed[@]} -gt 0 ]; then
    echo "Not found: ${failed[*]}"
    echo "Group opcodes need a REG suffix, for example 80.0 through 80.7."
fi

# Record what has been downloaded, so the test harness can enumerate opcodes
# without scanning the directory.
( cd "${DEST}" && ls -1 *.MOO 2>/dev/null > downloaded.txt )

# Flatten the flag masks into a plain table, so the test harness does not need
# a JSON parser. One line per opcode: "<opcode> <mask in hex>".
python3 - "${DEST}" <<'PYTHON'
import json, sys, os
dest = sys.argv[1]
opcodes = json.load(open(os.path.join(dest, "metadata.json")))["opcodes"]
lines = []
for opcode, entry in sorted(opcodes.items()):
    if not isinstance(entry, dict):
        continue
    lines.append("%s %04X" % (opcode, entry.get("flags-mask", 0xFFFF)))
    for reg, sub in sorted(entry.get("reg", {}).items()):
        if isinstance(sub, dict):
            lines.append("%s.%s %04X" % (opcode, reg, sub.get("flags-mask", 0xFFFF)))
with open(os.path.join(dest, "flags_masks.txt"), "w") as f:
    f.write("\n".join(lines) + "\n")
print("Wrote %d flag masks" % len(lines))
PYTHON

echo "Tests downloaded to ${DEST}"
