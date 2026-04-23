#pragma once              // include guard
#include <string>         // std::string
#include <memory>         // std::unique_ptr
#include <vector>         // std::vector
#include <variant>        // std::variant
#include <set>            // std::set for DFS
#include <cstdint>        // uint64_t

class Spreadsheet; // forward declaration; formula needs access to sheet

// Result of evaluating a formula: a number or an error string
using FormulaResult = std::variant<double, std::string>;

// ── AST node hierarchy ──────────────────────────────────────────────────────

struct EvalCtx; // forward declare evaluation context

struct ASTNode { // base AST node
    virtual ~ASTNode() = default;         // virtual destructor
    virtual double eval(EvalCtx& ctx) = 0; // evaluate this node
};

struct NumberNode : ASTNode { // numeric literal
    double val{0.0};                        // the literal value
    double eval(EvalCtx& ctx) override;     // returns val
};

struct CellRefNode : ASTNode { // single cell reference, e.g. B3
    int row{0}; // 0-based row
    int col{0}; // 0-based column
    double eval(EvalCtx& ctx) override; // resolves cell in spreadsheet
};

struct BinaryOpNode : ASTNode { // binary arithmetic expression
    char op{'+'}; // operator: +, -, *, /
    std::unique_ptr<ASTNode> left;  // left operand
    std::unique_ptr<ASTNode> right; // right operand
    double eval(EvalCtx& ctx) override; // applies operator
};

struct FunctionNode : ASTNode { // function call, e.g. SUM(A1:B5)
    std::string name; // function name (e.g. "SUM")
    // Range form (SUM(A1:B5))
    bool isRange{false}; // true when a cell range was supplied
    int r1{0}, c1{0};    // range start (0-based)
    int r2{0}, c2{0};    // range end (0-based)
    // Expression-list form (SUM(expr, expr, ...))
    std::vector<std::unique_ptr<ASTNode>> args; // arg expressions
    double eval(EvalCtx& ctx) override; // evaluates function
};

// Context passed down during evaluation (holds DFS state)
struct EvalCtx {
    Spreadsheet&         sheet;        // spreadsheet being evaluated
    std::set<uint64_t>&  vis;          // cells currently on the DFS stack
    std::set<uint64_t>&  done;         // cells whose evaluation is finished
    bool                 err{false};   // true when any error occurred
    bool                 cyclic{false};// true when a circular reference was found
};

// Evaluate a formula expression string (without the leading '=')
// Returns a double on success or an error string on failure
FormulaResult evaluateFormula(
    const std::string& expr, // expression text
    Spreadsheet&       sheet, // spreadsheet context
    std::set<uint64_t>& vis, // DFS visiting set
    std::set<uint64_t>& done  // DFS done set
);
