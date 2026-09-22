#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")/.."

set -e

CC="${CC:-gcc}"

for f in core/src/*/*.c; do
  $CC -std=c99 -fsyntax-only -Wall -Wextra -Wpedantic -Werror -Icore "$f"
done
