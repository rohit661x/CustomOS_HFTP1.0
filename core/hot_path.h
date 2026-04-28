
// core/hot_path.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "book_state.h"
#include "decision.h"

// Attribute helpers
#if defined(__GNUC__)
#define HOT   __attribute__((hot))
#define RESTRICT __restrict__
#else
#define HOT
#define RESTRICT
#endif

// Branchless extract/update/compute API (Slice 6)
typedef struct {
    const uint8_t* RESTRICT pkt;
    size_t len;
} itch_view_t;

HOT void extract_itch_fields(const itch_view_t* v, uint64_t* side, uint64_t* shares, uint64_t* price, uint64_t* sym8);
HOT void update_book_state(book_state_t* RESTRICT bs, uint64_t side, uint64_t shares, uint64_t price, uint64_t sym8);
HOT void compute_decision(const book_state_t* RESTRICT bs, decision_t* RESTRICT out);
