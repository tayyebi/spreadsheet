// =============================================================================
// sum.cpp  —  SUM formula function implementation
// =============================================================================
#include "sum.h"

// fn_SUM — add together all numeric values collected from a range or list.
//
// Step 1: collect values.
//   If node.isRange is true the user wrote "SUM(A1:B3)" and we use
//   collectRange() to pull every cell in the rectangle.
//   Otherwise the user wrote "SUM(A1, 5, C2)" and we use collectArgs()
//   to evaluate each argument expression in order.
//
// Step 2: accumulate.
//   A simple running total starting at 0.  An empty range/list returns 0,
//   which matches Excel/Calc behaviour (SUM of nothing = 0).
double fn_SUM(FunctionNode& node, EvalCtx& ctx) {
    auto v = node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);

    double total = 0;
    for (double x : v) total += x;
    return total;
}
