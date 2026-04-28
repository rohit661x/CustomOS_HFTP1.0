#!/usr/bin/env bash
set -euo pipefail

IMG="build/hftos.img"
UART_IMG="build/uart_test.img"
RUN_DIR="runs"
BASELINE="baseline.json"

mkdir -p "$RUN_DIR"

if [[ -f "$IMG" ]]; then
  BOOT="$IMG"
else
  BOOT="$UART_IMG"
fi

if [[ ! -f "$BOOT" ]]; then
  echo "No image found. Build one first (make uart_test.img or make hftos.img)" >&2
  exit 1
fi

ts=$(date +%s)
out="$RUN_DIR/run_${ts}.json"
log="$RUN_DIR/serial_${ts}.log"

echo "[perf_ci] Running QEMU with $BOOT ..."
set +e
qemu-system-x86_64 -drive file="$BOOT",format=raw,if=ide -nographic -serial stdio -no-reboot 2>&1 | tee "$log"
rc=$?
set -e

# Extract last PERF_JSON line if any
pj=$(grep -a "PERF_JSON:" "$log" | tail -n1 | sed 's/^.*PERF_JSON://')
if [[ -z "${pj}" ]]; then
  echo "[perf_ci] No PERF_JSON line detected in serial output."
  echo "{}" > "$out"
else
  echo "$pj" > "$out"
fi

if [[ -f "$BASELINE" ]]; then
  echo "[perf_ci] Comparing against baseline.json"
  python3 ci/ci_compare.py "$BASELINE" "$out"
  exit $?
else
  echo "[perf_ci] No baseline.json present. Consider bootstrapping one from $out"
  exit 0
fi
