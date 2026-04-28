
// obs/pipeline_trace.h (Slice 10)
#pragma once
#include <stdint.h>
#include "timing.h"

typedef struct {
    timing_sample_t ts;
    uint64_t per_stage[STAGE_COUNT];
    uint64_t total;
} pipeline_trace_t;

void analyze_pipeline_budget(const timing_sample_t* ts, pipeline_trace_t* out);
