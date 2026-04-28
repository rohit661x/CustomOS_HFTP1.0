
// matrix/config_matrix.h (Slice 13)
#pragma once
#include <stdint.h>

typedef struct {
    const char* config_name;
    uint8_t hw_filter_mode;   // 0=SW, 1=NIC_DROP, 2=MEM_DROP
    uint8_t fence_strategy;   // 0=aggressive,1=conservative,2=nuclear
    uint8_t doorbell_wc;      // 0=UC, 1=WC
    uint8_t pin_core;         // 0/1
    uint8_t cache_warm;       // 0/1
} test_config_t;
