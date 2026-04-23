// =============================================================================
// power.h  —  Declaration of the POWER formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_POWER — raise a base to an arbitrary exponent
//
// Syntax:  =POWER(base, exponent)
//
// Examples:
//   =POWER(2, 10)   → 1024
//   =POWER(9, 0.5)  → 3   (same as SQRT(9))
//   =POWER(2, -1)   → 0.5
//
// Returns #ERR! if fewer than 2 arguments are provided (via ctx.err).
// Note: POWER(negative, fractional) will produce NaN via std::pow; that is
// propagated as a numeric value here (display behaviour is implementation-
// defined for NaN in ostringstream).
double fn_POWER(FunctionNode& node, EvalCtx& ctx);
