
// obs/display_manager.h (Slice 7)
#pragma once
#include <stdint.h>

void tui_clear(void);
void tui_banner(const char* config_banner);
void tui_histogram(const uint64_t* buckets, int nb);
void tui_stage_breakdown(const uint64_t* per_stage, int n);
void tui_footer_p50_p99(uint64_t p50, uint64_t p99);
