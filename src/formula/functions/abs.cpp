#include "abs.h"
#include <cmath>
// ABS: absolute value of a single argument
double fn_ABS(FunctionNode&node,EvalCtx&ctx){
    if(node.args.empty()){ctx.err=true;return 0;}
    double v=node.args[0]?node.args[0]->eval(ctx):0;
    return std::abs(v);}
