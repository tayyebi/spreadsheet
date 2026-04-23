// =============================================================================
// abs.cpp  —  ABS formula function implementation
// =============================================================================
#include "abs.h"
#include <cmath>  // std::abs (the floating-point overload)

// fn_ABS — return the absolute value (non-negative magnitude) of the argument.
//
// std::abs is included from <cmath> for the double overload.
// The version in <cstdlib> operates on integers, so the <cmath> include
// is important to avoid silent truncation.
double fn_ABS(FunctionNode& node, EvalCtx& ctx) {
    // ABS requires exactly one argument.
    if (node.args.empty()) { ctx.err = true; return 0; }

    // Evaluate the argument expression (which may itself be a formula).
    double v = node.args[0] ? node.args[0]->eval(ctx) : 0;

    return std::abs(v);  // removes the sign; -7.5 → 7.5, 3 → 3
}
