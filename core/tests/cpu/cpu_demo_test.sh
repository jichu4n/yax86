#!/bin/bash

root_dir="$(dirname "$0")/../../.."
cd "$root_dir" || exit 1

cpu_demo=./build-native/core/tools/cpu_demo/cpu_demo
asm_dir=./core/tools/cpu_demo

set -xe

echo -e '12\n25\n' | $cpu_demo $asm_dir/horoscope.asm | grep Capricorn
echo -e '12\n15\n' | $cpu_demo $asm_dir/horoscope.asm | grep Sagittarius
echo -e '05\n01\n' | $cpu_demo $asm_dir/horoscope.asm | grep Taurus

for i in {1..12}; do
  echo -e "2025\n${i}\n" | build-native/core/tools/cpu_demo/cpu_demo $asm_dir/cal.asm
done
