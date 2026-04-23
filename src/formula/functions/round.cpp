// =============================================================================
// round.cpp  —  ROUND formula function implementation
// =============================================================================
#include "round.h"
#include <cmath>  // std::round, std::pow

// fn_ROUND — round `value` to `digits` decimal places.
//
// Technique: "multiply-round-divide"
//   1. Compute the scaling factor p = 10^digits.
//      E.g. digits=2 → p=100; digits=-1 → p=0.1
//   2. Multiply the value by p to shift the decimal point right.
//   3. Apply std::round() which rounds to the nearest integer (halfway rounds
//      away from zero, matching spreadsheet convention).
//   4. Divide by p to shift the decimal point back.
//
// We use std::round(n) on the digits argument itself to guard against
// floating-point imprecision in the digits value (e.g. 2.0000000001 → 2).
double fn_ROUND(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.size() < 2) { ctx.err = true; return 0; }

    double v = node.args[0] ? node.args[0]->eval(ctx) : 0;  // the value to round
    double n = node.args[1] ? node.args[1]->eval(ctx) : 0;  // number of decimal places

    // Scale factor: 10^n
    double p = std::pow(10.0, std::round(n));

    // Shift, round, shift back.
    return std::round(v * p) / p;
}
