
// matrix/config_implementation.h (Slice 13)
#pragma once
#include <stdint.h>
#include "config_matrix.h"

int apply_test_configuration(const test_config_t* c);
void warm_critical_cache_lines(void);
void flush_critical_cache_lines(void);
