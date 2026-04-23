// =============================================================================
// formula.h  —  Abstract Syntax Tree (AST) nodes, evaluation context,
//               and the public evaluateFormula() entry point
//
// When a user types "=1+SUM(A1:B3)*2" the formula engine does three things:
//   1. Lexing  — breaks the string into tokens (numbers, cell refs, operators…)
//   2. Parsing — assembles the tokens into a tree of ASTNode objects
//   3. Evaluation — walks the tree recursively to compute a result
//
// This header declares the tree nodes and the context passed down through
// the recursive evaluation so that cycle-detection state is shared.
// =============================================================================
#pragma once

#include <string>    // std::string — for function names and error values
#include <memory>    // std::unique_ptr — owns child AST nodes (RAII)
#include <vector>    // std::vector — holds argument lists in FunctionNode
#include <variant>   // std::variant — FormulaResult is double or error string
#include <set>       // std::set — cycle-detection sets passed by reference
#include <cstdint>   // uint64_t — cell key type used in EvalCtx sets

// Forward-declare Spreadsheet so we can reference it without a full include.
// (Including core.h here would create a mutual-include cycle because core.cpp
// includes formula.h.)
class Spreadsheet;

// ---------------------------------------------------------------------------
// FormulaResult  —  what a formula evaluation may return
//
// Either a double (successful numeric result) or a std::string (error token).
// Error strings always start with '#', e.g. "#ERR!", "#CYCLE!", "#DIV/0!".
// ---------------------------------------------------------------------------
using FormulaResult = std::variant<double, std::string>;

// Forward-declare EvalCtx; defined below after the node structs.
struct EvalCtx;

// ---------------------------------------------------------------------------
// ASTNode  —  the abstract base class for all expression tree nodes
//
// Every node knows how to evaluate itself given the current context.
// The virtual destructor ensures proper cleanup of node subclass objects
// even when held through a base-class pointer (required for unique_ptr).
// ---------------------------------------------------------------------------
struct ASTNode {
    virtual ~ASTNode() = default;
    // Evaluate this node and return a numeric result.
    // May set ctx.err or ctx.cyclic as side effects on error.
    virtual double eval(EvalCtx& ctx) = 0;
};

// ---------------------------------------------------------------------------
// NumberNode  —  a numeric literal, e.g. 42 or 3.14
//
// The parser creates one of these for every bare number in the formula.
// ---------------------------------------------------------------------------
struct NumberNode : ASTNode {
    double val = 0;                       // the literal value
    double eval(EvalCtx&) override;       // just returns val
};

// ---------------------------------------------------------------------------
// CellRefNode  —  a reference to another cell, e.g. A1 or B3
//
// During evaluation, we first ensure the referenced cell is evaluated
// (via Spreadsheet::evalCell), then return its numeric value.
// If the referenced cell contains an error, we propagate that error.
// ---------------------------------------------------------------------------
struct CellRefNode : ASTNode {
    int row = 0, col = 0;                 // zero-based grid coordinates
    double eval(EvalCtx& ctx) override;
};

// ---------------------------------------------------------------------------
// BinaryOpNode  —  an arithmetic operation: +, -, *, /
//
// Holds the operator character and owns two child subtrees (left, right).
// Evaluation is: left->eval() OP right->eval().
// Division by zero sets ctx.err = true and returns 0.
// ---------------------------------------------------------------------------
struct BinaryOpNode : ASTNode {
    char op = '+';                              // '+', '-', '*', or '/'
    std::unique_ptr<ASTNode> left, right;       // operand subtrees
    double eval(EvalCtx& ctx) override;
};

// ---------------------------------------------------------------------------
// FunctionNode  —  a named spreadsheet function call, e.g. SUM(A1:B3)
//
// Supports two calling conventions:
//   Range form:   SUM(A1:B3) — a rectangular block of cells
//   Arg-list form: SUM(A1,B1,C1) — an explicit list of expressions
//
// The `isRange` flag selects between the two.  For the range form the
// top-left (r1,c1) and bottom-right (r2,c2) corners are stored.
// For the arg-list form, `args` holds the evaluated sub-expressions.
//
// FunctionNode::eval() is defined in dispatch.cpp so that new functions
// can be added by editing dispatch.cpp without touching this header.
// ---------------------------------------------------------------------------
struct FunctionNode : ASTNode {
    std::string name;                             // upper-case, e.g. "SUM"
    bool isRange = false;                         // true when A1:B3 syntax used
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;         // range corners (isRange==true)
    std::vector<std::unique_ptr<ASTNode>> args;   // argument list (isRange==false)
    double eval(EvalCtx& ctx) override;
};

// ---------------------------------------------------------------------------
// EvalCtx  —  evaluation context passed through the entire DFS
//
// All data that the recursive eval() calls need to share is bundled here:
//   sheet   – the Spreadsheet, so cell references can trigger evalCell()
//   vis     – DFS visit stack: cells currently being evaluated (cycle detect)
//   done    – set of fully-evaluated cells (memoisation / avoid re-work)
//   err     – set to true by any node that encounters an evaluation error
//   cyclic  – set to true specifically when a circular reference is found
// ---------------------------------------------------------------------------
struct EvalCtx {
    Spreadsheet&      sheet;    // the grid; referenced via non-owning reference
    std::set<uint64_t>& vis;   // cells currently on the DFS call stack
    std::set<uint64_t>& done;  // cells already fully evaluated
    bool err    = false;        // any error occurred (division by zero, bad ref…)
    bool cyclic = false;        // circular dependency detected
};

// ---------------------------------------------------------------------------
// evaluateFormula()  —  parse and evaluate a formula expression string
//
// Parameters:
//   e    – the formula text WITHOUT the leading '=' (e.g. "1+SUM(A1:A3)")
//   s    – the Spreadsheet, forwarded into EvalCtx
//   vis  – DFS visit set, forwarded from evalCell()
//   done – DFS done set, forwarded from evalCell()
//
// Returns:
//   double      — the numeric result on success
//   std::string — an error token ("#ERR!" or "#CYCLE!") on failure
// ---------------------------------------------------------------------------
FormulaResult evaluateFormula(const std::string& e, Spreadsheet& s,
                               std::set<uint64_t>& vis, std::set<uint64_t>& done);
