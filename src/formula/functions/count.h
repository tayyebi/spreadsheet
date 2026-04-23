// =============================================================================
// count.h  —  Declaration of the COUNT formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_COUNT — count how many numeric cells exist in a range or argument list
//
// Supported syntax:
//   =COUNT(A1:B3)        — number of cells in the rectangle that hold a number
//   =COUNT(A1, B2, 3)   — number of arguments that evaluate to a number
//
// Text cells, empty cells, and error cells are NOT counted; only double values.
// This mirrors Excel's COUNT() behaviour (as opposed to COUNTA which counts text).
double fn_COUNT(FunctionNode& node, EvalCtx& ctx);
