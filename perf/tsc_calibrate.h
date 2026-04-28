
// perf/tsc_calibrate.h (Slice 7)
#pragma once
#include <stdint.h>

// Calibrate TSC to Hz via short busy loop (stub)
// Replace with your preferred method; log result to UART and stash globally.
uint64_t calibrate_tsc_frequency(void);
