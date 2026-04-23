// =============================================================================
// avg.h  —  Declaration of the AVERAGE formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_AVERAGE — compute the arithmetic mean (sum / count) of all values
//
// Supported syntax:
//   =AVERAGE(A1:B3)          — mean of the rectangle A1..B3
//   =AVERAGE(A1, B2, 3)     — mean of an explicit list
//
// Returns #ERR! (via ctx.err) if there are no numeric values to average,
// which avoids a division-by-zero result.
double fn_AVERAGE(FunctionNode& node, EvalCtx& ctx);
