
// perf/perf_counters.h (Slice 8)
#pragma once
#include <stdint.h>
int  setup_perf_counters(void);
int  read_perf_counters(uint64_t* ports, int n);
