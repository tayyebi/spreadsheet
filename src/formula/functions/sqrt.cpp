// =============================================================================
// sqrt.cpp  —  SQRT formula function implementation
// =============================================================================
#include "sqrt.h"
#include <cmath>  // std::sqrt (floating-point overload)

// fn_SQRT — return the square root of the argument.
//
// We explicitly check for negative input and raise an error rather than
// letting std::sqrt silently return NaN (which would propagate as a garbage
// number rather than a visible "#ERR!" in the cell).
double fn_SQRT(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }

    double v = node.args[0] ? node.args[0]->eval(ctx) : 0;

    // Square root of a negative number is not defined in the real numbers.
    // Signal an error so the cell shows "#ERR!" instead of silently
    // producing NaN which would display as something undefined.
    if (v < 0) { ctx.err = true; return 0; }

    return std::sqrt(v);
}
