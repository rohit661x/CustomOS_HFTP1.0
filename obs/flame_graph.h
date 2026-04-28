
// obs/flame_graph.h (Slice 10)
#pragma once
#include <stdint.h>
#include "pipeline_trace.h"

// Prepare a simple text flame for UART (last packet view)
void render_flame_graph_uart(const pipeline_trace_t* pt);
