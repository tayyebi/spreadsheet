#include "sqrt.h"
#include <cmath>
// SQRT: square root of a single argument
double fn_SQRT(FunctionNode&node,EvalCtx&ctx){
    if(node.args.empty()){ctx.err=true;return 0;}
    double v=node.args[0]?node.args[0]->eval(ctx):0;
    if(v<0){ctx.err=true;return 0;}
    return std::sqrt(v);}
