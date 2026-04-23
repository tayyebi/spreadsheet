// =============================================================================
// max.cpp  —  MAX formula function implementation
// =============================================================================
#include "max.h"
#include <limits>  // std::numeric_limits<double>::lowest()

// fn_MAX — return the largest value in the collected set.
//
// We initialise the running maximum to the most negative representable double
// (numeric_limits<double>::lowest(), which is approximately -1.8e308) so
// that any real value in the data will be larger and replace it immediately.
//
// An empty input signals an error for the same reason as MIN.
double fn_MAX(FunctionNode& node, EvalCtx& ctx) {
    auto v = node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);

    if (v.empty()) { ctx.err = true; return 0; }

    double m = std::numeric_limits<double>::lowest();
    for (double x : v)
        if (x > m) m = x;
    return m;
}
