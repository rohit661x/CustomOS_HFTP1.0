
// core/decision.h
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t  should_buy;
    uint8_t  should_sell;
    uint64_t order_price;
    uint32_t order_size;
} decision_t;
