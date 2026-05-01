// =============================================================================
// function_base.h  —  Shared helpers used by every formula function
//
// Rather than duplicating the "collect all the numbers from this range/list"
// logic inside SUM, AVERAGE, MIN, MAX, etc., we centralise it here.
// Every function file includes this header to get access to the two helpers.
// =============================================================================
#pragma once

#include "formula.h"  // FunctionNode, EvalCtx, ASTNode
#include "spreadsheet.h"  // Spreadsheet, Cell — needed to read cell values
#include <vector>     // std::vector — returned by both helpers

// ---------------------------------------------------------------------------
// collectRange()  —  gather all numeric values from a rectangular cell range
//
// Iterates every (row, col) pair inside the rectangle [r1,r2] × [c1,c2],
// evaluates cells that haven't been computed yet, and pushes each double
// value into the result vector.  Non-numeric and still-being-evaluated
// (cyclic) cells are silently skipped.
//
// Parameters:
//   ctx    – shared evaluation context (sheet reference, vis/done sets)
//   r1,c1  – top-left corner (zero-based, inclusive)
//   r2,c2  – bottom-right corner (zero-based, inclusive)
// ---------------------------------------------------------------------------
std::vector<double> collectRange(EvalCtx& ctx, int r1, int c1, int r2, int c2);

// ---------------------------------------------------------------------------
// collectArgs()  —  evaluate each argument expression and collect the results
//
// For functions called with an explicit argument list — e.g. SUM(A1, B2, 3)
// — each element of `args` is an ASTNode subtree.  We evaluate them in
// order and push the resulting doubles into the vector.
//
// Parameters:
//   ctx  – shared evaluation context
//   args – the vector of argument AST nodes from FunctionNode::args
// ---------------------------------------------------------------------------
std::vector<double> collectArgs(EvalCtx& ctx,
                                 const std::vector<std::unique_ptr<ASTNode>>& args);
