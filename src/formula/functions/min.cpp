// =============================================================================
// min.cpp  —  MIN formula function implementation
// =============================================================================
#include "min.h"
#include <limits>  // std::numeric_limits<double>::max()

// fn_MIN — return the smallest value in the collected set.
//
// We initialise the running minimum to the largest representable double
// (numeric_limits<double>::max()) so that any real value in the data will
// be smaller and replace it on the first iteration.  This avoids the need
// to special-case the first element.
//
// An empty input signals an error; returning 0 silently would hide mistakes.
double fn_MIN(FunctionNode& node, EvalCtx& ctx) {
    auto v = node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);

    if (v.empty()) { ctx.err = true; return 0; }

    double m = std::numeric_limits<double>::max();
    for (double x : v)
        if (x < m) m = x;
    return m;
}
