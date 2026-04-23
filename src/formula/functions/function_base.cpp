#include "function_base.h"
// Collect all numeric values from a rectangular cell range
std::vector<double> collectRange(EvalCtx&ctx,int r1,int c1,int r2,int c2){
    std::vector<double>v;
    for(int rr=r1;rr<=r2;++rr)for(int cc=c1;cc<=c2;++cc){
        auto k=Spreadsheet::key(rr,cc);if(ctx.vis.count(k))continue;
        ctx.sheet.evalCell(rr,cc,ctx.vis,ctx.done);
        const Cell*c=ctx.sheet.getCell(rr,cc);
        if(c&&std::holds_alternative<double>(c->value))v.push_back(std::get<double>(c->value));}
    return v;}
// Collect numeric results of evaluated argument expressions
std::vector<double> collectArgs(EvalCtx&ctx,const std::vector<std::unique_ptr<ASTNode>>&args){
    std::vector<double>v;
    for(const auto&a:args)if(a)v.push_back(a->eval(ctx));
    return v;}
