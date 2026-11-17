#!/bin/bash
# Fetch and build the Multi-Floppy BIOS Extension option ROM from source.
#
#   https://github.com/skiselev/floppy_bios
#
# Pinned to commit 4d2398f (version 2.7; the repository has no tagged
# releases). Assembles floppy_bios.asm with NASM, runs the repository's
# fix_checksum over the code and configuration regions, verifies that the
# output matches the recorded SHA-256 and the upstream prebuilt floppy_bios.bin
# byte for byte, and writes core/src/fdc/Multi_Floppy_BIOS_2.7.rom.
#
# Written for bash 3.2, the /bin/bash macOS ships.

set -e

readonly REPO_URL="https://github.com/skiselev/floppy_bios.git"
readonly COMMIT="4d2398f94af1f8efa2c9f3339971d85491feb193"
readonly EXPECTED_SHA256="c620a7c7faa75d509f0ce5eecfa958c4f969ec9d82fe962838a24307f21729a5"
readonly CACHE_DIR=".cache/floppy_bios"
readonly OUTPUT_ROM="core/src/fdc/Multi_Floppy_BIOS_2.7.rom"

cd "$(dirname "$0")"/..
mkdir -p .cache

if [ ! -d "${CACHE_DIR}/.git" ]; then
    git clone "${REPO_URL}" "${CACHE_DIR}"
fi

if [ "$(git -C "${CACHE_DIR}" rev-parse HEAD 2>/dev/null)" != "${COMMIT}" ]; then
    git -C "${CACHE_DIR}" fetch origin "${COMMIT}"
    git -C "${CACHE_DIR}" checkout "${COMMIT}"
fi

build_dir="$(mktemp -d)"
trap 'rm -rf "${build_dir}"' EXIT

"${CC:-cc}" -O2 -o "${build_dir}/fix_checksum" "${CACHE_DIR}/fix_checksum.c"
(
    cd "${CACHE_DIR}"
    nasm -O9 -f bin -o "${build_dir}/floppy_bios.bin" floppy_bios.asm
)
"${build_dir}/fix_checksum" \
    "${build_dir}/floppy_bios.bin" \
    "${build_dir}/floppy_bios.bin" \
    0 1FBF 1FBF 1FC0 1FFF 1FC0

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha256="$(sha256sum "${build_dir}/floppy_bios.bin" | awk '{print $1}')"
else
    actual_sha256="$(shasum -a 256 "${build_dir}/floppy_bios.bin" | awk '{print $1}')"
fi

if [ "${actual_sha256}" != "${EXPECTED_SHA256}" ]; then
    echo "SHA-256 mismatch: got ${actual_sha256}, expected ${EXPECTED_SHA256}" >&2
    exit 1
fi

if ! cmp -s "${build_dir}/floppy_bios.bin" "${CACHE_DIR}/floppy_bios.bin"; then
    echo "Built ROM differs from upstream floppy_bios.bin at ${COMMIT}" >&2
    exit 1
fi

cp "${build_dir}/floppy_bios.bin" "${OUTPUT_ROM}"
echo "Built ${OUTPUT_ROM} (SHA-256 ${actual_sha256}, matches upstream floppy_bios.bin)"
