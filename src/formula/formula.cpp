// =============================================================================
// formula.cpp  —  Lexer, parser, and AST node evaluations for formula strings
//
// This file turns a raw formula string (e.g. "1+SUM(A1:A3)*2") into a tree
// of ASTNode objects and then evaluates that tree.
//
// Pipeline:
//   string  →  [Lex]  →  token stream  →  [Par]  →  AST  →  [eval()]  →  double
//
// Design pattern used: Recursive Descent Parser.
//   The grammar is:
//     expr  ::= term  (('+' | '-') term)*
//     term  ::= fac   (('*' | '/') fac)*
//     fac   ::= '-' fac | '(' expr ')' | NUMBER | REF | FUNC '(' args ')'
// =============================================================================

#include "formula.h"  // AST declarations, EvalCtx, FormulaResult
#include "spreadsheet.h"  // Spreadsheet, Cell — needed for CellRefNode::eval
#include <cctype>     // isdigit, isupper, isspace — character classification
#include <stdexcept>  // std::stod, std::stoi may throw std::invalid_argument

// =============================================================================
// SECTION 1: LEXER (Tokeniser)
//
// The lexer breaks the formula string into a sequence of tokens.  A token is
// the smallest meaningful unit: a number, a cell reference, a function name,
// an operator character, a parenthesis, or a separator (colon / comma).
// =============================================================================

// TT — Token Type enumeration
// Each variant represents one class of input the parser must handle.
enum TT {
    NUM,  // numeric literal, e.g. 42 or 3.14
    REF,  // cell reference, e.g. A1 or BC12 (upper-case letters then digits)
    FUN,  // function name, e.g. SUM, AVERAGE (upper-case letters, no trailing digit)
    ADD,  // '+'
    SUB,  // '-'
    MUL,  // '*'
    DIV,  // '/'
    LP,   // '(' left parenthesis
    RP,   // ')' right parenthesis
    COL,  // ':' colon, separates range corners in A1:B3
    COM,  // ',' comma, separates function arguments
    END,  // end of input
    ERR   // unrecognised character — signals a parse error
};

// Tok — a single token
// `t` identifies the type; `n` carries the numeric value for NUM tokens;
// `s` carries the text for REF and FUN tokens.
struct Tok {
    TT          t = END;
    double      n = 0;
    std::string s;
};

// Convenience factory: make a Tok of a given type with no payload.
static Tok T(TT t) { return {t, 0, {}}; }

// ---------------------------------------------------------------------------
// Lex  —  the lexer struct
//
// Holds a reference to the source string and a position cursor `p`.
// next() is called repeatedly to consume one token at a time.
// ---------------------------------------------------------------------------
struct Lex {
    const std::string& src;  // formula text being tokenised
    size_t p = 0;            // current position in src

    // Return the next token and advance `p` past it.
    Tok next() {
        // Skip whitespace between tokens.
        while (p < src.size() && src[p] == ' ') ++p;

        // End of input.
        if (p >= src.size()) return T(END);

        char c = src[p];  // look at the current character

        // ------------------------------------------------------------------
        // Numeric literal: starts with a digit or a decimal point.
        // We collect the full number string and convert with stod.
        // ------------------------------------------------------------------
        if (isdigit((unsigned)c) || c == '.') {
            size_t b = p;
            while (p < src.size() && (isdigit((unsigned)src[p]) || src[p] == '.'))
                ++p;
            Tok t = T(NUM);
            try {
                t.n = std::stod(src.substr(b, p - b));
            } catch (...) {
                return T(ERR);  // malformed number
            }
            return t;
        }

        // ------------------------------------------------------------------
        // Identifier: starts with an uppercase letter.
        // Could be either a cell reference (letters + digits, e.g. A1, BC12)
        // or a function name (letters only, e.g. SUM, AVERAGE).
        // ------------------------------------------------------------------
        if (isupper((unsigned)c)) {
            std::string s;
            // Consume all uppercase letters.
            while (p < src.size() && isupper((unsigned)src[p]))
                s += src[p++];
            // If digits follow immediately, it's a cell reference like "A1".
            if (p < src.size() && isdigit((unsigned)src[p])) {
                while (p < src.size() && isdigit((unsigned)src[p]))
                    s += src[p++];
                return {REF, 0, s};
            }
            // No trailing digits → function name like "SUM".
            return {FUN, 0, s};
        }

        // ------------------------------------------------------------------
        // Single-character operators and delimiters.
        // ------------------------------------------------------------------
        ++p;  // consume the character
        switch (c) {
            case '+': return T(ADD);
            case '-': return T(SUB);
            case '*': return T(MUL);
            case '/': return T(DIV);
            case '(': return T(LP);
            case ')': return T(RP);
            case ':': return T(COL);
            case ',': return T(COM);
            default:  return T(ERR);  // unknown character
        }
    }
};

// =============================================================================
// SECTION 2: CELL REFERENCE PARSER
//
// Converts the A1-style cell reference string into zero-based (row, col).
// Column letters are base-26 encoded: A=0, B=1, …, Z=25, AA=26, …
// Row numbers are 1-based in the formula string but stored 0-based internally.
// =============================================================================
static bool pref(const std::string& s, int& row, int& col) {
    if (s.empty()) return false;

    // Decode column letters using base-26 positional notation.
    // "A"  → col=1, then col-- → 0
    // "B"  → col=2, then col-- → 1
    // "AA" → col=1*26+1=27, then col-- → 26
    col = 0;
    size_t i = 0;
    while (i < s.size() && isupper((unsigned)s[i])) {
        col = col * 26 + (s[i] - 'A' + 1);
        ++i;
    }
    col--;  // convert from 1-based to 0-based

    // Must have at least one digit after the letters.
    if (i >= s.size() || !isdigit((unsigned)s[i])) return false;

    // Decode the row number (1-based in the formula → 0-based internally).
    try {
        row = std::stoi(s.substr(i)) - 1;
    } catch (...) {
        return false;
    }

    // Validate bounds: must be within the maximum addressable grid.
    return row >= 0 && col >= 0
        && row < Spreadsheet::MAX_ROWS
        && col < Spreadsheet::MAX_COLS;
}

// =============================================================================
// SECTION 3: AST NODE EVALUATORS
//
// Each ASTNode subclass implements eval() by either returning a stored value
// (NumberNode), fetching a cell value (CellRefNode), combining two children
// (BinaryOpNode), or dispatching to a named function (FunctionNode, in dispatch.cpp).
// =============================================================================

// NumberNode::eval — a literal number needs no computation; just return it.
double NumberNode::eval(EvalCtx&) { return val; }

// ---------------------------------------------------------------------------
// CellRefNode::eval — fetch the value of another cell
//
// Steps:
//  1. Detect if the target cell is already on the DFS stack (cycle).
//  2. Recursively evaluate the target cell if not yet done.
//  3. Extract and return its numeric value, propagating errors if needed.
// ---------------------------------------------------------------------------
double CellRefNode::eval(EvalCtx& ctx) {
    auto k = Spreadsheet::key(row, col);

    // If the target cell is currently being evaluated (it is in `vis`),
    // we have found a circular reference.  Mark it and signal the error.
    if (ctx.vis.count(k)) {
        Cell* c = ctx.sheet.getCell(row, col);
        if (c && !ctx.done.count(k)) {
            c->display = "#CYCLE!";
            c->value   = std::string("#CYCLE!");
            ctx.done.insert(k);
        }
        ctx.err = ctx.cyclic = true;
        return 0;
    }

    // Recursively evaluate the target cell (DFS).
    ctx.sheet.evalCell(row, col, ctx.vis, ctx.done);

    const Cell* cell = ctx.sheet.getCell(row, col);
    if (!cell) return 0;  // cell was never written: treat as zero

    // If the cell holds a double, return it.
    if (std::holds_alternative<double>(cell->value))
        return std::get<double>(cell->value);

    // If the cell holds a string, check whether it is an error token.
    // Propagate error state so the parent formula also fails cleanly.
    if (std::holds_alternative<std::string>(cell->value)) {
        const auto& sv = std::get<std::string>(cell->value);
        if (!sv.empty() && sv[0] == '#') {
            ctx.err = true;
            if (sv == "#CYCLE!") ctx.cyclic = true;
        }
    }

    return 0;  // non-numeric text cells contribute 0 to arithmetic
}

// ---------------------------------------------------------------------------
// BinaryOpNode::eval — arithmetic on two sub-expressions
//
// We evaluate both operands first, then apply the operator.
// If either child already set ctx.err, we return 0 immediately to avoid
// cascading errors (e.g. "division by zero" masking the real cause).
// ---------------------------------------------------------------------------
double BinaryOpNode::eval(EvalCtx& ctx) {
    double l = left  ? left->eval(ctx)  : 0;
    double r = right ? right->eval(ctx) : 0;
    if (ctx.err) return 0;  // stop on first error

    switch (op) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/':
            if (r == 0) { ctx.err = true; return 0; }  // division by zero
            return l / r;
        default:
            ctx.err = true;
            return 0;
    }
}

// =============================================================================
// SECTION 4: RECURSIVE DESCENT PARSER
//
// Converts the token stream produced by the lexer into an AST.
// The grammar enforces standard arithmetic precedence:
//   * and / bind tighter than + and -.
//
//   expr  →  term { ('+' | '-') term }
//   term  →  fac  { ('*' | '/') fac }
//   fac   →  '-' fac
//           | '(' expr ')'
//           | NUMBER
//           | REF
//           | FUNC '(' [args] ')'
// =============================================================================

// N is a short alias for "owned AST node pointer" to keep parser code terse.
using N = std::unique_ptr<ASTNode>;

// Build a BinaryOpNode from an operator and two child nodes.
static N mkBin(char o, N l, N r) {
    auto b   = std::make_unique<BinaryOpNode>();
    b->op    = o;
    b->left  = std::move(l);
    b->right = std::move(r);
    return b;
}

// ---------------------------------------------------------------------------
// Par  —  the parser
//
// Holds the Lex object, the current (lookahead) token, and the EvalCtx.
// The three mutually-recursive methods expr/term/fac implement the grammar.
// ---------------------------------------------------------------------------
struct Par {
    Lex      lex;   // token source
    Tok      cur;   // current lookahead token
    EvalCtx& ctx;   // shared evaluation context (for error signalling)

    // Constructor: initialise the lexer and consume the first token.
    explicit Par(const std::string& s, EvalCtx& c) : lex{s}, ctx(c) { adv(); }

    // Advance: consume the current token and load the next one.
    void adv() { cur = lex.next(); }

    // -----------------------------------------------------------------------
    // expr  —  handle addition and subtraction (lowest precedence)
    // -----------------------------------------------------------------------
    N expr() {
        auto n = term();
        // Keep consuming + or - and building binary nodes (left-associative).
        while (!ctx.err && (cur.t == ADD || cur.t == SUB)) {
            char o = (cur.t == ADD) ? '+' : '-';
            adv();
            n = mkBin(o, std::move(n), term());
        }
        return n;
    }

    // -----------------------------------------------------------------------
    // term  —  handle multiplication and division (higher precedence)
    // -----------------------------------------------------------------------
    N term() {
        auto n = fac();
        while (!ctx.err && (cur.t == MUL || cur.t == DIV)) {
            char o = (cur.t == MUL) ? '*' : '/';
            adv();
            n = mkBin(o, std::move(n), fac());
        }
        return n;
    }

    // -----------------------------------------------------------------------
    // fac  —  handle atoms and unary minus (highest precedence)
    // -----------------------------------------------------------------------
    N fac() {
        // Unary minus: "-X" becomes BinaryOpNode(0 - X).
        if (cur.t == SUB) {
            adv();
            auto z = std::make_unique<NumberNode>();  // implicit 0
            return mkBin('-', std::move(z), fac());
        }

        // Parenthesised sub-expression: consume '(', parse expr, consume ')'.
        if (cur.t == LP) {
            adv();
            auto n = expr();
            if (cur.t == RP) adv();
            return n;
        }

        // Numeric literal: create a NumberNode with the parsed value.
        if (cur.t == NUM) {
            auto n = std::make_unique<NumberNode>();
            n->val = cur.n;
            adv();
            return n;
        }

        // Cell reference (e.g. A1): create a CellRefNode.
        if (cur.t == REF) {
            auto n = std::make_unique<CellRefNode>();
            if (!pref(cur.s, n->row, n->col)) { ctx.err = true; return nullptr; }
            adv();
            return n;
        }

        // Function call (e.g. SUM(...)):
        //   • FUNC '(' REF ':' REF ')'    — range form: SUM(A1:B3)
        //   • FUNC '(' expr {',' expr} ')' — arg-list form: SUM(A1,B1,2)
        if (cur.t == FUN) {
            std::string fn = cur.s;
            adv();
            if (cur.t != LP) { ctx.err = true; return nullptr; }
            adv();  // consume '('

            auto f  = std::make_unique<FunctionNode>();
            f->name = fn;

            // Try to detect range syntax: FUNC(REF : REF)
            if (cur.t == REF) {
                std::string ss = cur.s;
                adv();
                if (cur.t == COL) {
                    // Range form: A1:B3
                    adv();
                    if (cur.t != REF) { ctx.err = true; return nullptr; }
                    if (!pref(ss, f->r1, f->c1) || !pref(cur.s, f->r2, f->c2)) {
                        ctx.err = true;
                        return nullptr;
                    }
                    adv();
                    f->isRange = true;
                } else {
                    // First argument was a cell ref, not a range — arg-list form.
                    auto rn = std::make_unique<CellRefNode>();
                    if (!pref(ss, rn->row, rn->col)) { ctx.err = true; return nullptr; }
                    f->args.push_back(std::move(rn));
                    // Consume any additional comma-separated arguments.
                    while (cur.t == COM) { adv(); f->args.push_back(expr()); }
                }
            } else if (cur.t != RP) {
                // Arg-list form starting with a non-REF expression.
                f->args.push_back(expr());
                while (cur.t == COM) { adv(); f->args.push_back(expr()); }
            }

            if (cur.t == RP) adv();  // consume closing ')'
            return f;
        }

        // Nothing matched — set error flag and return null.
        ctx.err = true;
        return nullptr;
    }
};

// =============================================================================
// SECTION 5: PUBLIC ENTRY POINT
//
// evaluateFormula() is the only function called from outside this file.
// It stitches together the lexer, parser, and evaluator.
// =============================================================================
FormulaResult evaluateFormula(const std::string& expr, Spreadsheet& s,
                               std::set<uint64_t>& vis, std::set<uint64_t>& done) {
    // An empty formula string evaluates to 0 (no-op).
    if (expr.empty()) return 0.0;

    // Build the shared evaluation context, then parse the expression.
    EvalCtx ctx{s, vis, done};
    Par     p(expr, ctx);
    auto    tree = p.expr();  // builds the full AST

    // If the parser hit an error or returned a null tree, report "#ERR!".
    if (ctx.err || !tree) return std::string("#ERR!");

    // Walk the AST recursively; eval() updates ctx.err / ctx.cyclic as needed.
    double r = tree->eval(ctx);

    // Map the error flags to appropriate error strings.
    if (ctx.cyclic) return std::string("#CYCLE!");
    if (ctx.err)    return std::string("#ERR!");
    return r;
}
