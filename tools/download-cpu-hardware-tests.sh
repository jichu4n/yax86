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
# Pinned to a commit rather than tracking a branch. The suite has no tags or
# releases, and the results it records are what the tests compare against - so
# upstream changing them would change what CI means with no commit here. The
# expected number of known divergences in hardware_test.cpp is tied to this
# revision as well.
readonly COMMIT="aea84484abc79d09639d855b7b0ab32bc9e4dbeb"
readonly BASE_URL="https://raw.githubusercontent.com/${REPO}/${COMMIT}"
readonly DEST=".cache/8088-tests"
readonly OPCODE_LIST="tools/cpu-hardware-tests-opcodes.txt"
# Files are small and numerous, so fetch several at once.
readonly PARALLEL_DOWNLOADS=8

cd "$(dirname "$0")"/..
mkdir -p "${DEST}"

if [ $# -gt 0 ]; then
    opcodes=("${@^^}")
else
    # Read the opcode list from the repository rather than listing the
    # directory over the GitHub API, which is rate limited to 60 requests an
    # hour per IP for anonymous callers - a limit CI runners share.
    mapfile -t opcodes < <(grep -v '^#' "${OPCODE_LIST}" | grep -v '^[[:space:]]*$')
fi

# Only fetch what is not already here. The data is pinned to one revision, so
# a file that is present is the file that is wanted, and running this a second
# time has nothing to do - which is what lets tools/run-tests.sh call it every
# time without paying for it.
missing=()
for opcode in "${opcodes[@]}"; do
    if [ ! -s "${DEST}/${opcode}.MOO.gz" ]; then
        missing+=("${opcode}.MOO.gz")
    fi
done

# The metadata lists which opcodes are undocumented and, for each, a mask of
# the flags the 8088 leaves undefined. Tests are compared with those bits
# cleared, so that emulating undefined behavior is not required.
if [ ! -s "${DEST}/metadata.json" ]; then
    echo "Downloading metadata..."
    curl -fsSL "${BASE_URL}/v2/metadata.json" -o "${DEST}/metadata.json.part"
    mv "${DEST}/metadata.json.part" "${DEST}/metadata.json"
fi

if [ ${#missing[@]} -gt 0 ]; then
    echo "Downloading ${#missing[@]} opcode files to ${DEST}..."
    # Each file lands under a temporary name and is moved into place only once
    # curl has succeeded. An interrupted run then leaves nothing half written
    # for the next one to mistake for a complete file and skip.
    printf '%s\n' "${missing[@]}" |
        xargs -P "${PARALLEL_DOWNLOADS}" -I {} sh -c '
            curl -fsS --retry 3 "$1/v2_binary/$2" -o "$3/$2.part" &&
                mv "$3/$2.part" "$3/$2"
        ' _ "${BASE_URL}" {} "${DEST}"
fi

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

PYTHON

echo "${#opcodes[@]} opcode files present ($(du -sh "${DEST}" | cut -f1))" \
     "in ${DEST}, ${#missing[@]} newly downloaded"
