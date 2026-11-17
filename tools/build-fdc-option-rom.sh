#!/bin/bash
# Fetch and build the Multi-Floppy BIOS Extension option ROM from source, with
# the emulator patch applied.
#
#   https://github.com/skiselev/floppy_bios
#
# Pinned to commit 4d2398f (version 2.7; the repository has no tagged
# releases). Builds the ROM three times from a copy of the source, assembling
# floppy_bios.asm with NASM and running the repository's fix_checksum over the
# code and configuration regions each time:
#
#   1. Upstream as it is, which must match the upstream prebuilt
#      floppy_bios.bin byte for byte: the source is what the ROM came from.
#   2. With tools/floppy_bios-emu.patch applied but EMULATOR not defined,
#      which must match it too: the patch changes nothing outside EMULATOR.
#   3. With the patch and -DEMULATOR, which must match EXPECTED_SHA256, and is
#      written to core/src/fdc/Multi_Floppy_BIOS_2.7_emu.rom.
#
# The patch makes the ROM wait for the floppy controller's interrupt with HLT
# rather than by polling the PIT, and skips the head settle time after a seek,
# as GLaBIOS's ARCH_EMU build does. See "fdc" in core/AGENTS.md.
#
# Written for bash 3.2, the /bin/bash macOS ships.

set -e

readonly REPO_URL="https://github.com/skiselev/floppy_bios.git"
readonly COMMIT="4d2398f94af1f8efa2c9f3339971d85491feb193"
readonly EXPECTED_SHA256="a93b33fa547f06112c6289ac454d721969b22a61a15f1e89065ccba7e48ea63e"
readonly CACHE_DIR=".cache/floppy_bios"
readonly PATCH="tools/floppy_bios-emu.patch"
readonly OUTPUT_ROM="core/src/fdc/Multi_Floppy_BIOS_2.7_emu.rom"

cd "$(dirname "$0")"/..
readonly ROOT="$(pwd)"
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

# Copies the pinned source into ${build_dir}/$1, never touching the cache.
copy_source() {
    mkdir "${build_dir}/$1"
    cp "${CACHE_DIR}"/*.asm "${CACHE_DIR}"/*.inc "${build_dir}/$1/"
}

# Assembles ${build_dir}/$1 into ${build_dir}/$1.bin, with any further
# arguments passed to NASM.
assemble() {
    local name="$1"
    shift
    (
        cd "${build_dir}/${name}"
        nasm -O9 -f bin "$@" -o "${build_dir}/${name}.bin" floppy_bios.asm
    )
    "${build_dir}/fix_checksum" \
        "${build_dir}/${name}.bin" \
        "${build_dir}/${name}.bin" \
        0 1FBF 1FBF 1FC0 1FFF 1FC0 >/dev/null
}

# Fails unless ${build_dir}/$1.bin is upstream's floppy_bios.bin.
check_matches_upstream() {
    if ! cmp -s "${build_dir}/$1.bin" "${CACHE_DIR}/floppy_bios.bin"; then
        echo "$2 differs from upstream floppy_bios.bin at ${COMMIT}" >&2
        exit 1
    fi
}

copy_source upstream
assemble upstream
check_matches_upstream upstream "The unpatched build"

copy_source patched
(cd "${build_dir}/patched" && git apply "${ROOT}/${PATCH}")
assemble patched
check_matches_upstream patched "The patched build without EMULATOR"

assemble patched -DEMULATOR

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha256="$(sha256sum "${build_dir}/patched.bin" | awk '{print $1}')"
else
    actual_sha256="$(shasum -a 256 "${build_dir}/patched.bin" | awk '{print $1}')"
fi

if [ "${actual_sha256}" != "${EXPECTED_SHA256}" ]; then
    echo "SHA-256 mismatch: got ${actual_sha256}, expected ${EXPECTED_SHA256}" >&2
    exit 1
fi

cp "${build_dir}/patched.bin" "${OUTPUT_ROM}"
echo "Built ${OUTPUT_ROM} (SHA-256 ${actual_sha256})"
