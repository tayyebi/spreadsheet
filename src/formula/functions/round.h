// =============================================================================
// round.h  —  Declaration of the ROUND formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_ROUND — round a value to a specified number of decimal places
//
// Syntax:  =ROUND(value, digits)
//
// Examples:
//   =ROUND(3.14159, 2)  → 3.14
//   =ROUND(3.14159, 0)  → 3
//   =ROUND(3.14159, -1) → 0   (rounds to nearest 10; 3 rounds to 0)
//   =ROUND(1234.5, -2)  → 1200
//
// The `digits` argument may be negative to round to tens, hundreds, etc.
// Returns #ERR! if fewer than 2 arguments are provided (via ctx.err).
double fn_ROUND(FunctionNode& node, EvalCtx& ctx);
