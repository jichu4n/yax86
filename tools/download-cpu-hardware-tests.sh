#!/bin/bash
# Download the 8088 hardware-generated CPU test suite.
#
#   https://github.com/SingleStepTests/8088
#
# Downloads every opcode by default, which is about 480MB. Files are left
# compressed, as they expand to several times that and the test harness reads
# them through zlib.
#
# A subset can be fetched instead, which is useful when iterating on one
# instruction. Group opcodes are split per ModRM REG value:
#
#   ./tools/download-cpu-hardware-tests.sh 8D 00 80.0

set -e

readonly REPO="SingleStepTests/8088"
readonly BASE_URL="https://raw.githubusercontent.com/${REPO}/main"
readonly DEST=".cache/8088-tests"
# Files are small and numerous, so fetch several at once.
readonly PARALLEL_DOWNLOADS=8

cd "$(dirname "$0")"/..
mkdir -p "${DEST}"

# The metadata lists which opcodes are undocumented and, for each, a mask of
# the flags the 8088 leaves undefined. Tests are compared with those bits
# cleared, so that emulating undefined behavior is not required.
echo "Downloading metadata..."
curl -fsSL "${BASE_URL}/v2/metadata.json" -o "${DEST}/metadata.json"

if [ $# -gt 0 ]; then
    files=()
    for opcode in "$@"; do
        files+=("${opcode^^}.MOO.gz")
    done
else
    echo "Listing available opcodes..."
    # The contents API returns every entry in the directory in one response.
    mapfile -t files < <(
        curl -fsSL "https://api.github.com/repos/${REPO}/contents/v2_binary" |
            python3 -c "
import json, sys
for entry in json.load(sys.stdin):
    if entry['name'].endswith('.MOO.gz'):
        print(entry['name'])
"
    )
fi

echo "Downloading ${#files[@]} opcode files to ${DEST}..."
printf '%s\n' "${files[@]}" |
    xargs -P "${PARALLEL_DOWNLOADS}" -I {} \
        curl -fsS --retry 3 "${BASE_URL}/v2_binary/{}" -o "${DEST}/{}"

# Record what is present, so the test harness can enumerate opcodes without
# scanning the directory.
(
    cd "${DEST}"
    ls -1 ./*.MOO.gz 2>/dev/null | sed 's|^\./||' > downloaded.txt
)

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
            mask = sub.get("flags-mask", 0xFFFF)
            lines.append("%s.%s %04X" % (opcode, reg, mask))
with open(os.path.join(dest, "flags_masks.txt"), "w") as f:
    f.write("\n".join(lines) + "\n")
print("Wrote %d flag masks" % len(lines))
PYTHON

echo "Downloaded $(wc -l < "${DEST}/downloaded.txt") opcode files," \
     "$(du -sh "${DEST}" | cut -f1) in ${DEST}"
