#!/bin/bash
# run_variant.sh <uf2> <label> [key] [budget]
set -u
UF2=$1; LABEL=$2; KEY=${3:-4}; BUDGET=${4:-180}
echo "=============== $LABEL ==============="
picotool reboot -u -f > /dev/null 2>&1
sleep 3
picotool load -x "$UF2" > /dev/null 2>&1 || { echo "load failed"; exit 1; }
sleep 3
python3 "$(dirname "$0")/capture.py" "$KEY" "$BUDGET"
