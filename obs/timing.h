
// obs/timing.h (Slices 7/10)
#pragma once
#include <stdint.h>

// Ensure an ordered TSC read (serialize with LFENCE on Intel)
static inline uint64_t rdtsc_ordered(void) {
    unsigned lo, hi;
    __asm__ __volatile__("lfence; rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

typedef enum {
    STAGE_POLL=0,
    STAGE_LOAD,
    STAGE_PARSE,
    STAGE_FILTER,
    STAGE_BOOK,
    STAGE_DECISION,
    STAGE_ORDER,
    STAGE_TXDESC,
    STAGE_FENCE,
    STAGE_DOORBELL,
    STAGE_CONFIRM,
    STAGE_TOTAL,
    STAGE_COUNT
} stage_t;

typedef struct {
    uint64_t tap[STAGE_COUNT+1]; // +1 for end
} timing_sample_t;

#define START_PACKET_TIMING(ts) do { (ts).tap[0] = rdtsc_ordered(); } while(0)
#define TAP(ts, stage)          do { (ts).tap[(stage)+1] = rdtsc_ordered(); } while(0)
#define END_PACKET_TIMING(ts)   do { (ts).tap[STAGE_COUNT] = rdtsc_ordered(); } while(0)

static inline uint64_t stage_cycles(const timing_sample_t* ts, stage_t s) {
    return ts->tap[s+1] - ts->tap[s];
}
