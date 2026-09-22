#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -e

NM="${NM:-nm}"
LIB="build-native/core/libyax86_core.a"
SYMBOLS_FILE="core/public_symbols.txt"
UPDATE=false

for arg in "$@"; do
  case "$arg" in
    --update)
      UPDATE=true
      ;;
    -*)
      echo "Unknown option: $arg" >&2
      echo "Usage: $0 [--update] [path/to/libyax86_core.a]" >&2
      exit 1
      ;;
    *)
      LIB="$arg"
      ;;
  esac
done

if [ ! -f "$LIB" ]; then
  echo "Error: $LIB not found. Please build the project first." >&2
  exit 1
fi

get_symbols() {
  "$NM" -g "$LIB" \
    | awk '{
        type = "";
        name = "";
        if ($2 ~ /^[TDBRG]$/) {
          type = $2;
          name = $3;
        } else if ($1 ~ /^[TDBRG]$/) {
          type = $1;
          name = $2;
        }
        if (type != "") {
          # Strip leading underscore for macOS / BSD nm portability.
          sub(/^_/, "", name);
          print type " " name;
        }
      }' \
    | LC_ALL=C sort -k2,2 -k1,1 -u
}

if [ "$UPDATE" = true ]; then
  get_symbols > "$SYMBOLS_FILE"
  echo "Updated $SYMBOLS_FILE ($(wc -l < "$SYMBOLS_FILE" | tr -d ' ') symbols)"
  exit 0
fi

if [ ! -f "$SYMBOLS_FILE" ]; then
  echo "Error: Allowlist file $SYMBOLS_FILE not found." >&2
  exit 1
fi

if ! diff -u "$SYMBOLS_FILE" <(get_symbols); then
  echo "" >&2
  echo "Error: Exported symbols in $LIB do not match $SYMBOLS_FILE." >&2
  echo "If this change is intentional, update the allowlist with:" >&2
  echo "  tools/check-public-symbols.sh --update" >&2
  exit 1
fi
