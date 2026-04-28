
// obs/uart_stats.h (Slice 7)
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t count;
    uint64_t sum;
    uint64_t sumsq;
    uint64_t p50;
    uint64_t p99;
} stats_t;

void stats_init(stats_t* s);
void stats_add(stats_t* s, uint64_t v);
void stats_emit_line(const stats_t* s);
