#!/usr/bin/env bash
# tools/validate_uart_t580.sh — sanity check UART path on host
set -euo pipefail

echo "This demo uses QEMU -serial stdio. For real HW tests, ensure /dev/ttyS0 exists and permissions are OK."
