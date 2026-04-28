#!/usr/bin/env bash
# tools/t580_demo_setup.sh — hygiene for more deterministic runs on Linux (host)
set -euo pipefail

echo "[setup] Setting CPU governor to performance (requires root)"
if command -v cpupower >/dev/null 2>&1; then
  sudo cpupower frequency-set -g performance || true
fi

echo "[setup] Disabling intel_pstate powersave C-states (temporary)"
# These may vary by distro; adapt as needed
if [[ -d /sys/devices/system/cpu/cpuidle ]]; then
  for s in /sys/devices/system/cpu/cpuidle/state*/disable; do
    echo 1 | sudo tee "$s" >/dev/null || true
  done
fi

echo "[setup] Disabling irqbalance"
sudo systemctl stop irqbalance || true

echo "[setup] Done. Remember to reverse these after testing."
