// =============================================================================
// count.cpp  —  COUNT formula function implementation
// =============================================================================
#include "count.h"

// fn_COUNT — return the number of numeric values in the range or list.
//
// collectRange / collectArgs already filter out non-numeric cells — they only
// push a value when the cell holds a double.  Therefore the size of the
// returned vector IS the count of numeric cells/arguments.
//
// We cast to double because all formula results are doubles; the cell will
// display "3" not "3.0" because Spreadsheet uses ostringstream which omits
// the fractional part for whole-number doubles.
double fn_COUNT(FunctionNode& node, EvalCtx& ctx) {
    auto v = node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);

    // The vector contains only numeric values; its size is the answer.
    return static_cast<double>(v.size());
}
