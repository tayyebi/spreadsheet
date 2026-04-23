// =============================================================================
// abs.h  —  Declaration of the ABS formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_ABS — absolute value of a single expression
//
// Syntax:  =ABS(value)
//
// Examples:
//   =ABS(-7)   → 7
//   =ABS(A1)   → |A1|
//   =ABS(3-5)  → 2
//
// Returns #ERR! if no argument is provided (via ctx.err).
double fn_ABS(FunctionNode& node, EvalCtx& ctx);
