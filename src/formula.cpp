#include "formula.h" // own header
#include "core.h"    // Spreadsheet, Cell, CellValue
#include <cctype>    // isdigit, isupper, isspace
#include <cstring>   // strlen
#include <stdexcept> // std::stod

// ── Lexer ────────────────────────────────────────────────────────────────────

// Token types produced by the lexer
enum class TT {
    NUM,    // floating-point number literal
    REF,    // cell reference, e.g. "B3"
    FUNC,   // function name, e.g. "SUM"
    PLUS,   // +
    MINUS,  // -
    STAR,   // *
    SLASH,  // /
    LPAREN, // (
    RPAREN, // )
    COLON,  // : (range separator)
    COMMA,  // ,
    END,    // end of input
    ERR     // unrecognised character
};

struct Token {
    TT          type{TT::END}; // token kind
    double      num{0.0};      // value when type==NUM
    std::string str{};         // text  when type==REF or FUNC
};

struct Lexer {
    const std::string& src; // source expression
    size_t             pos{0}; // current character position

    // Advance past whitespace and return the next token
    Token next() {
        while (pos < src.size() && src[pos] == ' ') ++pos; // skip spaces
        if (pos >= src.size()) return Token{TT::END, 0.0, {}}; // end of input

        char c = src[pos]; // peek at current character

        // ── number literal ─────────────────────────────────────────────────
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = pos; // record start
            while (pos < src.size() &&
                   (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.'))
                ++pos; // consume digits and decimal point
            Token t{TT::NUM, 0.0, {}}; // build token
            try { t.num = std::stod(src.substr(start, pos - start)); } // parse
            catch (...) { return Token{TT::ERR, 0.0, {}}; } // malformed number
            return t; // return number token
        }

        // ── identifier: cell reference or function name ────────────────────
        if (std::isupper(static_cast<unsigned char>(c))) {
            std::string s; // accumulate letters
            while (pos < src.size() && std::isupper(static_cast<unsigned char>(src[pos])))
                s += src[pos++]; // consume uppercase letters
            if (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                    s += src[pos++]; // consume digits of row number
                return Token{TT::REF, 0.0, s}; // cell reference token
            }
            return Token{TT::FUNC, 0.0, s}; // function name token
        }

        ++pos; // consume single-character token
        switch (c) {            // map character to token type
            case '+': return Token{TT::PLUS,   0.0, {}}; // addition
            case '-': return Token{TT::MINUS,  0.0, {}}; // subtraction
            case '*': return Token{TT::STAR,   0.0, {}}; // multiplication
            case '/': return Token{TT::SLASH,  0.0, {}}; // division
            case '(': return Token{TT::LPAREN, 0.0, {}}; // open paren
            case ')': return Token{TT::RPAREN, 0.0, {}}; // close paren
            case ':': return Token{TT::COLON,  0.0, {}}; // range separator
            case ',': return Token{TT::COMMA,  0.0, {}}; // arg separator
            default:  return Token{TT::ERR,    0.0, {}}; // unknown char
        }
    }
};

// ── Cell reference parser helper ─────────────────────────────────────────────

// Convert A1-notation string to 0-based (row, col); returns false on failure
static bool parseRef(const std::string& s, int& row, int& col) {
    if (s.empty()) return false; // empty string is invalid
    col = 0;  // accumulate column index
    size_t i = 0; // position in string
    while (i < s.size() && std::isupper(static_cast<unsigned char>(s[i]))) {
        col = col * 26 + (s[i] - 'A' + 1); // shift left and add letter value
        ++i; // advance
    }
    col -= 1; // convert to 0-based
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i])))
        return false; // no row digits
    try { row = std::stoi(s.substr(i)) - 1; } // parse row, convert to 0-based
    catch (...) { return false; } // stoi failed
    return row >= 0 && col >= 0 && // validate bounds
           row < Spreadsheet::ROWS && col < Spreadsheet::COLS;
}

// ── Recursive-descent parser ─────────────────────────────────────────────────

struct Parser {
    Lexer    lex;  // tokeniser
    Token    cur;  // lookahead token
    EvalCtx& ctx;  // evaluation context

    explicit Parser(const std::string& s, EvalCtx& c) : lex{s}, ctx(c) {
        advance(); // prime lookahead
    }

    void advance() { cur = lex.next(); } // consume one token

    // expr := term (('+' | '-') term)*
    std::unique_ptr<ASTNode> parseExpr() {
        auto node = parseTerm(); // parse left-hand term
        while (!ctx.err && (cur.type == TT::PLUS || cur.type == TT::MINUS)) {
            char op = cur.type == TT::PLUS ? '+' : '-'; // record operator
            advance();                                    // consume operator
            auto rhs = parseTerm();                       // parse right term
            auto bin = std::make_unique<BinaryOpNode>();  // build node
            bin->op    = op;                              // store operator
            bin->left  = std::move(node);                 // left child
            bin->right = std::move(rhs);                  // right child
            node = std::move(bin);                        // promote
        }
        return node; // return expression node
    }

    // term := factor (('*' | '/') factor)*
    std::unique_ptr<ASTNode> parseTerm() {
        auto node = parseFactor(); // parse left-hand factor
        while (!ctx.err && (cur.type == TT::STAR || cur.type == TT::SLASH)) {
            char op = cur.type == TT::STAR ? '*' : '/'; // record operator
            advance();                                   // consume operator
            auto rhs = parseFactor();                    // parse right factor
            auto bin = std::make_unique<BinaryOpNode>(); // build node
            bin->op    = op;                             // store operator
            bin->left  = std::move(node);                // left child
            bin->right = std::move(rhs);                 // right child
            node = std::move(bin);                       // promote
        }
        return node; // return term node
    }

    // factor := '(' expr ')' | '-' factor | NUMBER | CELLREF | FUNC '(' ... ')'
    std::unique_ptr<ASTNode> parseFactor() {
        if (cur.type == TT::MINUS) {          // unary negation
            advance();                        // consume '-'
            auto inner = parseFactor();       // parse operand
            auto bin   = std::make_unique<BinaryOpNode>(); // negate via 0-x
            bin->op    = '-';                 // subtraction operator
            auto zero  = std::make_unique<NumberNode>(); // zero literal
            zero->val  = 0.0;                // value zero
            bin->left  = std::move(zero);    // left is 0
            bin->right = std::move(inner);   // right is operand
            return bin;                      // return negation node
        }
        if (cur.type == TT::LPAREN) {        // parenthesised expression
            advance();                       // consume '('
            auto node = parseExpr();         // parse inner expression
            if (cur.type == TT::RPAREN) advance(); // consume ')'
            return node;                     // return inner node
        }
        if (cur.type == TT::NUM) {           // numeric literal
            auto n  = std::make_unique<NumberNode>(); // build node
            n->val  = cur.num;               // set value
            advance();                       // consume token
            return n;                        // return number node
        }
        if (cur.type == TT::REF) {           // cell reference
            int row = 0, col = 0;            // destination
            if (!parseRef(cur.str, row, col)) { ctx.err = true; return nullptr; } // bad ref
            auto n  = std::make_unique<CellRefNode>(); // build node
            n->row  = row;                   // set row
            n->col  = col;                   // set column
            advance();                       // consume token
            return n;                        // return ref node
        }
        if (cur.type == TT::FUNC) {          // function call
            std::string fname = cur.str;     // function name
            advance();                       // consume name
            if (cur.type != TT::LPAREN) { ctx.err = true; return nullptr; } // expected '('
            advance();                       // consume '('
            auto fn = std::make_unique<FunctionNode>(); // build function node
            fn->name = fname;                // store name

            // Check for range syntax: FUNC(REF:REF)
            if (cur.type == TT::REF) {       // first token is a cell ref
                std::string startStr = cur.str; // remember start ref
                advance();                       // consume start ref
                if (cur.type == TT::COLON) {     // range separator found
                    advance();                   // consume ':'
                    if (cur.type != TT::REF) { ctx.err = true; return nullptr; } // expected end ref
                    std::string endStr = cur.str; // end ref
                    advance();                    // consume end ref
                    int r1=0,c1=0,r2=0,c2=0;     // range corners
                    if (!parseRef(startStr, r1, c1) || !parseRef(endStr, r2, c2)) {
                        ctx.err = true; return nullptr; // invalid range
                    }
                    fn->isRange = true; // mark as range
                    fn->r1 = r1; fn->c1 = c1; // store start
                    fn->r2 = r2; fn->c2 = c2; // store end
                } else {                         // single ref – treat as expression
                    fn->isRange = false;         // not a range
                    auto refNode = std::make_unique<CellRefNode>(); // make ref node
                    int rr=0,cc=0;               // parse reference
                    if (!parseRef(startStr, rr, cc)) { ctx.err = true; return nullptr; }
                    refNode->row = rr; refNode->col = cc; // assign
                    fn->args.push_back(std::move(refNode)); // add as first arg
                    while (cur.type == TT::COMMA) { // additional arguments
                        advance();            // consume ','
                        fn->args.push_back(parseExpr()); // parse next arg
                    }
                }
            } else if (cur.type != TT::RPAREN) { // expression arguments
                fn->isRange = false;              // not a range
                fn->args.push_back(parseExpr());  // first argument
                while (cur.type == TT::COMMA) {   // more arguments
                    advance();                    // consume ','
                    fn->args.push_back(parseExpr()); // parse next arg
                }
            }
            if (cur.type == TT::RPAREN) advance(); // consume ')'
            return fn;                             // return function node
        }
        ctx.err = true;   // unexpected token
        return nullptr;   // error node
    }
};

// ── AST node eval implementations ────────────────────────────────────────────

double NumberNode::eval(EvalCtx& /*ctx*/) { return val; } // return literal

double CellRefNode::eval(EvalCtx& ctx) {
    auto k = Spreadsheet::key(row, col);  // compute key
    if (ctx.vis.count(k)) {               // this cell is on the stack = cycle
        // Mark the cycle-root cell immediately so callers can see it
        Cell* c = ctx.sheet.getCell(row, col); // get root cell
        if (c && !ctx.done.count(k)) {         // not yet finalised
            c->display = "#CYCLE!";            // set display
            c->value   = std::string("#CYCLE!"); // set value
            ctx.done.insert(k);                // prevent re-entry
        }
        ctx.err    = true;  // flag error
        ctx.cyclic = true;  // flag cycle specifically
        return 0.0;         // sentinel
    }
    ctx.sheet.evalCell(row, col, ctx.vis, ctx.done); // ensure evaluated
    const Cell* cell = ctx.sheet.getCell(row, col);  // fetch cell
    if (!cell) return 0.0;                            // empty cell = 0
    if (std::holds_alternative<double>(cell->value))  // numeric value
        return std::get<double>(cell->value);          // return it
    if (std::holds_alternative<std::string>(cell->value)) { // string/error
        const auto& s = std::get<std::string>(cell->value); // get string
        if (!s.empty() && s[0] == '#') {              // error string
            ctx.err = true;                           // propagate error flag
            if (s == "#CYCLE!") ctx.cyclic = true;    // propagate cycle flag
        }
    }
    return 0.0; // non-numeric treated as 0
}

double BinaryOpNode::eval(EvalCtx& ctx) {
    double l = left  ? left->eval(ctx)  : 0.0; // evaluate left
    double r = right ? right->eval(ctx) : 0.0; // evaluate right
    if (ctx.err) return 0.0; // propagate error
    switch (op) { // apply operator
        case '+': return l + r; // addition
        case '-': return l - r; // subtraction
        case '*': return l * r; // multiplication
        case '/':               // division
            if (r == 0.0) { ctx.err = true; return 0.0; } // divide by zero
            return l / r; // result
        default: ctx.err = true; return 0.0; // unknown op
    }
}

double FunctionNode::eval(EvalCtx& ctx) {
    if (name == "SUM") { // SUM function
        double total = 0.0; // accumulator
        if (isRange) {      // range form SUM(A1:B5)
            for (int rr = r1; rr <= r2; ++rr) {       // rows in range
                for (int cc = c1; cc <= c2; ++cc) {   // columns in range
                    auto k = Spreadsheet::key(rr, cc); // cell key
                    if (ctx.vis.count(k)) continue;    // skip cycle cells
                    ctx.sheet.evalCell(rr, cc, ctx.vis, ctx.done); // evaluate
                    const Cell* cell = ctx.sheet.getCell(rr, cc);  // get cell
                    if (cell && std::holds_alternative<double>(cell->value))
                        total += std::get<double>(cell->value); // add to sum
                }
            }
        } else {            // expression-list form
            for (auto& arg : args) { // each argument
                if (arg) total += arg->eval(ctx); // add evaluated arg
            }
        }
        return total; // return total
    }
    ctx.err = true;   // unknown function
    return 0.0;       // error sentinel
}

// ── Public entry point ────────────────────────────────────────────────────────

FormulaResult evaluateFormula(const std::string& expr,
                               Spreadsheet&       sheet,
                               std::set<uint64_t>& vis,
                               std::set<uint64_t>& done) {
    if (expr.empty()) return 0.0; // empty formula = 0
    EvalCtx ctx{sheet, vis, done}; // build evaluation context
    Parser  p(expr, ctx);          // build parser with context
    auto    tree = p.parseExpr();  // parse into AST
    if (ctx.err || !tree) return std::string("#ERR!"); // parse error
    double result = tree->eval(ctx); // evaluate AST
    if (ctx.cyclic) return std::string("#CYCLE!"); // cycle error takes priority
    if (ctx.err) return std::string("#ERR!"); // general eval error
    return result; // return numeric result
}
