// =============================================================================
// avg.cpp  —  AVERAGE formula function implementation
// =============================================================================
#include "avg.h"

// fn_AVERAGE — arithmetic mean: sum(values) / count(values)
//
// We reuse the same collect helpers as SUM.
// Guard against an empty set: if no numeric values exist, set ctx.err so
// the cell displays "#ERR!" rather than a meaningless 0 or a division crash.
double fn_AVERAGE(FunctionNode& node, EvalCtx& ctx) {
    auto v = node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);

    // Cannot compute a mean of zero values — signal an error.
    if (v.empty()) { ctx.err = true; return 0; }

    double total = 0;
    for (double x : v) total += x;

    // Cast the count to double before dividing to avoid integer truncation.
    return total / static_cast<double>(v.size());
}
