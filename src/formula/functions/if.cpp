#include "if.h"
// IF: evaluate condition; return true_val if nonzero, else false_val
double fn_IF(FunctionNode&node,EvalCtx&ctx){
    if(node.args.size()<2){ctx.err=true;return 0;}
    double cond=node.args[0]?node.args[0]->eval(ctx):0;
    if(ctx.err)return 0;
    size_t idx=cond!=0.0?1u:2u;
    if(idx>=node.args.size())return 0;
    return node.args[idx]?node.args[idx]->eval(ctx):0;}
