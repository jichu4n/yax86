#!/bin/bash

root_dir="$(dirname "$0")/../.."
cd "$root_dir" || exit 1

cpu_demo=./build/demos/cpu/cpu_demo

set -xe

echo -e '12\n25\n' | build/demos/cpu/cpu_demo demos/cpu/horoscope.asm | grep Capricorn
echo -e '12\n15\n' | build/demos/cpu/cpu_demo demos/cpu/horoscope.asm | grep Sagittarius
echo -e '05\n01\n' | build/demos/cpu/cpu_demo demos/cpu/horoscope.asm | grep Taurus

for i in {1..12}; do
  echo -e "2025\n${i}\n" | build/demos/cpu/cpu_demo demos/cpu/cal.asm
done
