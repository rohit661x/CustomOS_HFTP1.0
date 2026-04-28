
// nic/intel_fdir.h (Slice 5)
#pragma once
#include <stdint.h>

typedef struct {
    const char* sym_ascii; // "AAPL"
    uint64_t    sym8;      // 8B padded cached
    uint32_t    payload_off;
    uint32_t    rule_id;
} fdir_rule_t;

int setup_aapl_flow_rule(void);
int program_flow_director_rule(const fdir_rule_t* r);
int configure_nic_filtering(void);
int validate_flow_rules(void);
