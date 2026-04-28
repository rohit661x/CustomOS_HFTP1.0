
// core/book_state.h
#pragma once
#include <stdint.h>
#include <stdalign.h>

typedef struct alignas(64) {
    uint64_t best_bid_px;
    uint64_t best_bid_sz;
    uint64_t best_ask_px;
    uint64_t best_ask_sz;
    uint8_t  _pad[64 - (4*sizeof(uint64_t))]; // pad to 64B
} book_state_t;

// 8-byte padded symbol constants
#define SYM_AAPL 0x2020204C50414141ULL  /* "AAPL    " little-endian for stub */
