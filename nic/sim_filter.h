
// nic/sim_filter.h (Slice 5)
#pragma once
#include <stdint.h>

// Runtime toggles simulated in QEMU env via memory hooks
enum {
    SIM_HW_FILTER_IN_NIC = 0,
    SIM_HW_FILTER_IN_MEM = 1,
    SW_FILTER_FALLBACK   = 2,
};

int should_process_packet(uint64_t sym8);
