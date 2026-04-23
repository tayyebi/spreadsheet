// =============================================================================
// if.h  —  Declaration of the IF formula function
// =============================================================================
#pragma once
#include "function_base.h"

// fn_IF — conditional: IF(condition, value_if_true, value_if_false)
//
// Syntax:
//   =IF(A1>0, 1, -1)    — evaluates the condition expression; if nonzero
//                          returns the true branch, otherwise the false branch
//
// NOTE: the condition is itself an arbitrary expression.  Any nonzero value
// is "true"; zero (or the result of an empty argument) is "false".
// The false branch argument is optional; if omitted the function returns 0.
//
// This is a "short-circuit" evaluation: only the selected branch is evaluated.
// The other branch expression is never executed (important for side-effect-
// free evaluation in a spreadsheet, though in practice it avoids unnecessary
// cell evaluations and potential spurious errors).
double fn_IF(FunctionNode& node, EvalCtx& ctx);
