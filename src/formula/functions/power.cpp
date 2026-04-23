// =============================================================================
// power.cpp  —  POWER formula function implementation
// =============================================================================
#include "power.h"
#include <cmath>  // std::pow

// fn_POWER — compute base raised to the power of exponent: base^exp.
//
// std::pow handles all sign and fractional-exponent combinations according
// to IEEE 754; see your C++ standard library documentation for edge cases
// (e.g. pow(0, 0) = 1, pow(-1, 0.5) = NaN).
double fn_POWER(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.size() < 2) { ctx.err = true; return 0; }

    double base = node.args[0] ? node.args[0]->eval(ctx) : 0;  // the base
    double exp  = node.args[1] ? node.args[1]->eval(ctx) : 0;  // the exponent

    return std::pow(base, exp);
}
