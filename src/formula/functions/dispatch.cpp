// =============================================================================
// dispatch.cpp  —  FunctionNode::eval() implementation
//
// This is the single "switchboard" for all formula functions.
// When the formula evaluator reaches a FunctionNode in the AST, it calls
// FunctionNode::eval().  That method looks up the function name in a static
// hash-map and delegates to the appropriate fn_XXX() implementation file.
//
// Design benefits of this approach:
//   • Adding a new function requires only: (a) a new fn_xxx.h/cpp file pair,
//     (b) one #include here, and (c) one entry in the map.
//   • The std::unordered_map gives O(1) average-case dispatch regardless of
//     how many functions are registered, unlike a chain of if/else comparisons.
//   • The map is `static const` so it is built exactly once (on first call).
// =============================================================================

#include "formula.h"   // FunctionNode, EvalCtx declarations
#include "sum.h"       // fn_SUM
#include "avg.h"       // fn_AVERAGE
#include "min.h"       // fn_MIN
#include "max.h"       // fn_MAX
#include "count.h"     // fn_COUNT
#include "if.h"        // fn_IF
#include "abs.h"       // fn_ABS
#include "round.h"     // fn_ROUND
#include "sqrt.h"      // fn_SQRT
#include "power.h"     // fn_POWER
#include <unordered_map>  // std::unordered_map — O(1) name→function lookup
#include <functional>     // (included transitively, but makes intent explicit)

// ---------------------------------------------------------------------------
// FunctionNode::eval()
//
// The function pointer type FnPtr matches the signature shared by every
// fn_XXX implementation:  double fn_XXX(FunctionNode&, EvalCtx&)
//
// `fns` maps the upper-case function name string to its implementation.
// It is `static const` so it is initialised once and never reallocated.
// ---------------------------------------------------------------------------
double FunctionNode::eval(EvalCtx& ctx) {
    // Each function implementation file exposes one free function with this
    // exact signature.  Storing raw function pointers avoids any heap
    // allocation that std::function would introduce.
    using FnPtr = double(*)(FunctionNode&, EvalCtx&);

    // Static initialisation: built on the first call, reused on every
    // subsequent call.  Thread-safe in C++11 and later (magic statics).
    static const std::unordered_map<std::string, FnPtr> fns = {
        {"SUM",     fn_SUM},      // =SUM(A1:A10) or =SUM(A1,B2,3)
        {"AVERAGE", fn_AVERAGE},  // =AVERAGE(A1:A5)
        {"MIN",     fn_MIN},      // =MIN(A1:C3)
        {"MAX",     fn_MAX},      // =MAX(A1:C3)
        {"COUNT",   fn_COUNT},    // =COUNT(A1:A10)  — counts numeric cells
        {"IF",      fn_IF},       // =IF(cond, true_val, false_val)
        {"ABS",     fn_ABS},      // =ABS(-5)  → 5
        {"ROUND",   fn_ROUND},    // =ROUND(3.14159, 2)  → 3.14
        {"SQRT",    fn_SQRT},     // =SQRT(9)   → 3
        {"POWER",   fn_POWER},    // =POWER(2,10) → 1024
    };

    // Look up the function name in the map.
    auto it = fns.find(name);
    if (it != fns.end())
        return it->second(*this, ctx);  // call the function implementation

    // Unknown function name: set the error flag and return 0.
    // The caller (evaluateFormula) will convert this into "#ERR!".
    ctx.err = true;
    return 0;
}
