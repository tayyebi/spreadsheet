// =============================================================================
// max.h  —  Declaration of the MAX formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_MAX — find the largest numeric value in a range or argument list
//
// Supported syntax:
//   =MAX(A1:B3)         — maximum cell value in the rectangle
//   =MAX(A1, B2, 100)  — maximum of an explicit list
//
// Returns #ERR! if no numeric values are available (via ctx.err).
double fn_MAX(FunctionNode& node, EvalCtx& ctx);
