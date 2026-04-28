
// ci/env_capture.h (Slice 15)
#pragma once
#include <stdint.h>
// Capture runtime env knobs into a struct to emit via PERF_JSON
typedef struct {
    const char* cpu_model;
    const char* governor;
    const char* cstate_mask;
    double      temp_c;
} env_info_t;
