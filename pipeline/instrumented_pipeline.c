
// pipeline/instrumented_pipeline.c (Slice 10)
#include <stdint.h>
#include "../obs/timing.h"
#include "../obs/pipeline_trace.h"
#include "../core/hot_path.h"

void analyze_pipeline_budget(const timing_sample_t* ts, pipeline_trace_t* out) {
    for (int s = 0; s < STAGE_COUNT; ++s) {
        out->per_stage[s] = stage_cycles(ts, (stage_t)s);
    }
    out->total = out->per_stage[STAGE_TOTAL]; // last delta
}
