#include "formula.h" // hdr
#include "core.h" // sheet
#include <cctype> // ctype
#include <stdexcept> // stod
enum TT{NUM,REF,FUN,ADD,SUB,MUL,DIV,LP,RP,COL,COM,END,ERR}; // kinds
struct Tok{TT t=END;double n=0;std::string s;}; // token
static Tok T(TT t){return{t,0,{}};} // helper
struct Lex{ // lexer
    const std::string&src;size_t p=0; // src, pos
    Tok next(){ // tokenize
        while(p<src.size()&&src[p]==' ')++p; // ws
        if(p>=src.size())return T(END); // eof
        char c=src[p]; // peek
        if(isdigit((unsigned)c)||c=='.'){ // number
            size_t b=p;while(p<src.size()&&(isdigit((unsigned)src[p])||src[p]=='.'))++p;
            Tok t=T(NUM);try{t.n=std::stod(src.substr(b,p-b));}catch(...){return T(ERR);}return t;} // parse
        if(isupper((unsigned)c)){ // ident
            std::string s;while(p<src.size()&&isupper((unsigned)src[p]))s+=src[p++]; // letters
            if(p<src.size()&&isdigit((unsigned)src[p])){while(p<src.size()&&isdigit((unsigned)src[p]))s+=src[p++];return{REF,0,s};} // ref
            return{FUN,0,s};} // func
        ++p;switch(c){ // op
            case'+':return T(ADD);case'-':return T(SUB);case'*':return T(MUL);case'/':return T(DIV);
            case'(':return T(LP);case')':return T(RP);case':':return T(COL);case',':return T(COM);
            default:return T(ERR);}}}; // err
static bool pref(const std::string&s,int&row,int&col){ // A1→r,c
    if(s.empty())return false; // empty
    col=0;size_t i=0;
    while(i<s.size()&&isupper((unsigned)s[i])){col=col*26+(s[i]-'A'+1);++i;} // col
    col--;if(i>=s.size()||!isdigit((unsigned)s[i]))return false; // check
    try{row=std::stoi(s.substr(i))-1;}catch(...){return false;} // row
    return row>=0&&col>=0&&row<Spreadsheet::ROWS&&col<Spreadsheet::COLS;} // ok
double NumberNode::eval(EvalCtx&){return val;} // literal
double CellRefNode::eval(EvalCtx&ctx){ // ref
    auto k=Spreadsheet::key(row,col); // key
    if(ctx.vis.count(k)){ // cycle
        Cell*c=ctx.sheet.getCell(row,col);
        if(c&&!ctx.done.count(k)){c->display="#CYCLE!";c->value=std::string("#CYCLE!");ctx.done.insert(k);} // mark
        ctx.err=ctx.cyclic=true;return 0;} // flag
    ctx.sheet.evalCell(row,col,ctx.vis,ctx.done); // eval
    const Cell*cell=ctx.sheet.getCell(row,col);if(!cell)return 0; // absent
    if(std::holds_alternative<double>(cell->value))return std::get<double>(cell->value); // num
    if(std::holds_alternative<std::string>(cell->value)){
        const auto&sv=std::get<std::string>(cell->value);
        if(!sv.empty()&&sv[0]=='#'){ctx.err=true;if(sv=="#CYCLE!")ctx.cyclic=true;}} // err
    return 0;} // zero
double BinaryOpNode::eval(EvalCtx&ctx){ // binop
    double l=left?left->eval(ctx):0,r=right?right->eval(ctx):0;if(ctx.err)return 0; // ops
    switch(op){case'+':return l+r;case'-':return l-r;case'*':return l*r;
        case'/':if(r==0){ctx.err=true;return 0;}return l/r;default:ctx.err=true;return 0;}} // arith
using N=std::unique_ptr<ASTNode>; // alias
static N mkBin(char o,N l,N r){auto b=std::make_unique<BinaryOpNode>();b->op=o;b->left=std::move(l);b->right=std::move(r);return b;} // build
struct Par{ // parser
    Lex lex;Tok cur;EvalCtx&ctx; // state
    explicit Par(const std::string&s,EvalCtx&c):lex{s},ctx(c){adv();} // ctor
    void adv(){cur=lex.next();} // advance
    N expr(){auto n=term();while(!ctx.err&&(cur.t==ADD||cur.t==SUB)){char o=cur.t==ADD?'+':'-';adv();n=mkBin(o,std::move(n),term());}return n;} // expr
    N term(){auto n=fac();while(!ctx.err&&(cur.t==MUL||cur.t==DIV)){char o=cur.t==MUL?'*':'/';adv();n=mkBin(o,std::move(n),fac());}return n;} // term
    N fac(){ // factor
        if(cur.t==SUB){adv();auto z=std::make_unique<NumberNode>();return mkBin('-',std::move(z),fac());} // neg
        if(cur.t==LP){adv();auto n=expr();if(cur.t==RP){adv();}return n;} // paren
        if(cur.t==NUM){auto n=std::make_unique<NumberNode>();n->val=cur.n;adv();return n;} // num
        if(cur.t==REF){auto n=std::make_unique<CellRefNode>();if(!pref(cur.s,n->row,n->col)){ctx.err=true;return nullptr;}adv();return n;} // ref
        if(cur.t==FUN){std::string fn=cur.s;adv();if(cur.t!=LP){ctx.err=true;return nullptr;}adv(); // func
            auto f=std::make_unique<FunctionNode>();f->name=fn; // node
            if(cur.t==REF){std::string ss=cur.s;adv(); // first ref
                if(cur.t==COL){adv();if(cur.t!=REF){ctx.err=true;return nullptr;} // range
                    if(!pref(ss,f->r1,f->c1)||!pref(cur.s,f->r2,f->c2)){ctx.err=true;return nullptr;} // check
                    adv();f->isRange=true;} // set
                else{auto rn=std::make_unique<CellRefNode>();if(!pref(ss,rn->row,rn->col)){ctx.err=true;return nullptr;} // ref
                    f->args.push_back(std::move(rn));while(cur.t==COM){adv();f->args.push_back(expr());}}} // args
            else if(cur.t!=RP){f->args.push_back(expr());while(cur.t==COM){adv();f->args.push_back(expr());}} // args
            if(cur.t==RP){adv();}return f;} // close
        ctx.err=true;return nullptr;}}; // err
FormulaResult evaluateFormula(const std::string&expr,Spreadsheet&s,std::set<uint64_t>&vis,std::set<uint64_t>&done){ // eval
    if(expr.empty())return 0.0; // empty
    EvalCtx ctx{s,vis,done}; // ctx
    Par p(expr,ctx);auto tree=p.expr(); // parse
    if(ctx.err||!tree)return std::string("#ERR!"); // err
    double r=tree->eval(ctx); // eval
    if(ctx.cyclic)return std::string("#CYCLE!");else if(ctx.err)return std::string("#ERR!");return r;} // done
