// =============================================================================
// function_base.cpp  —  Implementations of the shared range/arg collectors
// =============================================================================

#include "function_base.h"

// ---------------------------------------------------------------------------
// collectRange()  —  gather numeric values from a rectangular cell region
//
// We walk every cell in the rectangle row-by-row, column-by-column:
//
//   for each row rr in [r1 .. r2]:
//     for each col cc in [c1 .. c2]:
//       if cell is already on the DFS stack → skip (avoid false cycle)
//       otherwise: evaluate the cell, then read its numeric value
//
// Cells that hold text, errors, or are still monostate (unevaluated) are
// simply not pushed into `v` — they contribute nothing to numeric functions.
// ---------------------------------------------------------------------------
std::vector<double> collectRange(EvalCtx& ctx,
                                  int r1, int c1, int r2, int c2) {
    std::vector<double> v;
    for (int rr = r1; rr <= r2; ++rr) {
        for (int cc = c1; cc <= c2; ++cc) {
            auto k = Spreadsheet::key(rr, cc);
            // Skip cells that are currently being evaluated (they are on the
            // DFS stack in ctx.vis).  Including them would cause a false cycle.
            if (ctx.vis.count(k)) continue;

            // Ensure the cell has been evaluated before we try to read it.
            ctx.sheet.evalCell(rr, cc, ctx.vis, ctx.done);

            const Cell* c = ctx.sheet.getCell(rr, cc);
            // Only push numeric (double) values; text/error cells are ignored.
            if (c && std::holds_alternative<double>(c->value))
                v.push_back(std::get<double>(c->value));
        }
    }
    return v;
}

// ---------------------------------------------------------------------------
// collectArgs()  —  evaluate each argument expression and collect results
//
// Iterates the args vector in order.  Each element is an ASTNode; calling
// eval() on it runs the recursive tree-walk for that argument.  A null
// pointer (which can appear if the parser set an error on that argument)
// is silently skipped.
// ---------------------------------------------------------------------------
std::vector<double> collectArgs(EvalCtx& ctx,
                                 const std::vector<std::unique_ptr<ASTNode>>& args) {
    std::vector<double> v;
    for (const auto& a : args)
        if (a) v.push_back(a->eval(ctx));  // null-check guards against bad parse
    return v;
}
