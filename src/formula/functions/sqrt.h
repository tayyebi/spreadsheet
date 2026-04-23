// =============================================================================
// sqrt.h  —  Declaration of the SQRT formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_SQRT — compute the principal (non-negative) square root
//
// Syntax:  =SQRT(value)
//
// Examples:
//   =SQRT(9)   → 3
//   =SQRT(2)   → 1.41421...
//   =SQRT(-1)  → #ERR!  (square root of a negative is not a real number)
//
// Returns #ERR! if:
//   • no argument is given
//   • the argument evaluates to a negative number
double fn_SQRT(FunctionNode& node, EvalCtx& ctx);
