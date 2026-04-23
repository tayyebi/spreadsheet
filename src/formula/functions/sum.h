// =============================================================================
// sum.h  —  Declaration of the SUM formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_SUM — compute the sum of all values in a range or argument list
//
// Supported syntax:
//   =SUM(A1:B3)           — sum every cell in the rectangle A1..B3
//   =SUM(A1, B2, 3, C4)  — sum an explicit list of expressions
double fn_SUM(FunctionNode& node, EvalCtx& ctx);
