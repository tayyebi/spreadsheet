// =============================================================================
// if.cpp  —  IF formula function implementation
// =============================================================================
#include "if.h"

// fn_IF — short-circuit conditional evaluation.
//
// args[0] = condition expression  (required)
// args[1] = value returned when condition != 0  (required)
// args[2] = value returned when condition == 0  (optional; defaults to 0)
//
// We evaluate only the condition first.  If an error occurred during
// condition evaluation we bail out immediately (ctx.err is already set).
// Then we select which branch to evaluate based on the result.
double fn_IF(FunctionNode& node, EvalCtx& ctx) {
    // Require at least the condition and the true-branch arguments.
    if (node.args.size() < 2) { ctx.err = true; return 0; }

    // Evaluate the condition expression (args[0]).
    double cond = node.args[0] ? node.args[0]->eval(ctx) : 0;
    if (ctx.err) return 0;  // propagate errors from the condition itself

    // Choose branch: index 1 for true (cond != 0), index 2 for false (cond == 0).
    size_t idx = (cond != 0.0) ? 1u : 2u;

    // If the selected branch doesn't exist (e.g. user wrote =IF(1,99) with no
    // false branch and the condition is false), return 0 as a default.
    if (idx >= node.args.size()) return 0;

    // Evaluate and return only the selected branch.
    return node.args[idx] ? node.args[idx]->eval(ctx) : 0;
}
