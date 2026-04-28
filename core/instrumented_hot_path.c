
// core/instrumented_hot_path.c
#include <stdint.h>
#include <string.h>
#include "hot_path.h"
#include "../obs/timing.h"

// Dummy ITCH offsets for the stub (replace with real protocol)
enum { ITCH_SIDE_OFF=8, ITCH_SHARES_OFF=12, ITCH_PRICE_OFF=16, ITCH_SYMBOL_OFF=24 };

static inline uint64_t load_u32(const void* p) {
    uint32_t x;
    __builtin_memcpy(&x, p, sizeof(x));
    return (uint64_t)x;
}

void extract_itch_fields(const itch_view_t* v, uint64_t* side, uint64_t* shares, uint64_t* price, uint64_t* sym8) {
    // Branchless loads; endian swaps can be added if needed
    const uint8_t* base = v->pkt;
    *side   = load_u32(base + ITCH_SIDE_OFF);
    *shares = load_u32(base + ITCH_SHARES_OFF);
    *price  = load_u32(base + ITCH_PRICE_OFF);
    // load 8 bytes of symbol aligned or memmove if unknown alignment
    uint64_t tmp = 0;
    __builtin_memcpy(&tmp, base + ITCH_SYMBOL_OFF, 8);
    *sym8 = tmp;
}

void update_book_state(book_state_t* bs, uint64_t side, uint64_t shares, uint64_t price, uint64_t sym8) {
    // Filter to AAPL (8-byte padded constant) without a branch
    const uint64_t is_aapl = (sym8 == SYM_AAPL);
    const uint64_t is_bid  = (side & 1ULL); // stub encoding
    // cmov-like via conditional masking
    const uint64_t bid_px_new = is_aapl ? price : bs->best_bid_px;
    const uint64_t ask_px_new = is_aapl ? price : bs->best_ask_px;

    bs->best_bid_px = is_bid ? bid_px_new : bs->best_bid_px;
    bs->best_ask_px = is_bid ? bs->best_ask_px : ask_px_new;
    // Sizes elided; fill in once real ITCH msg types are wired
}

void compute_decision(const book_state_t* bs, decision_t* out) {
    // Simple arithmetic-only decision stub
    const int64_t spread = (int64_t)bs->best_ask_px - (int64_t)bs->best_bid_px;
    out->should_buy  = (spread > 0) & (spread < 5);  // stub threshold
    out->should_sell = 0;
    out->order_price = bs->best_bid_px + 1;
    out->order_size  = 1;
}
