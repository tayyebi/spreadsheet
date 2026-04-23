// =============================================================================
// min.h  —  Declaration of the MIN formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_MIN — find the smallest numeric value in a range or argument list
//
// Supported syntax:
//   =MIN(A1:B3)         — minimum cell value in the rectangle
//   =MIN(A1, B2, -5)   — minimum of an explicit list
//
// Returns #ERR! if no numeric values are available (via ctx.err).
double fn_MIN(FunctionNode& node, EvalCtx& ctx);
