
// obs/perf_emit.h (Slice 15 JSON emitter)
// Emit a single-line compact JSON on UART: "PERF_JSON:{...}\n"
#pragma once
#include <stdint.h>

typedef struct {
    // Fill with env/config as needed (slice 15)
    uint32_t fence_conservative;
    uint32_t doorbell_wc;
    uint32_t pin_core;
    uint32_t cache_warm;
    // stats
    uint64_t p50;
    uint64_t p99;
    uint64_t p999;
    double   cv;
    double   outlier_rate;
} perf_json_t;

void perf_emit_json(const perf_json_t* pj);
