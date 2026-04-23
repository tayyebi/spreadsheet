#include "round.h"
#include <cmath>
// ROUND: round value to N decimal places; ROUND(value, digits)
double fn_ROUND(FunctionNode&node,EvalCtx&ctx){
    if(node.args.size()<2){ctx.err=true;return 0;}
    double v=node.args[0]?node.args[0]->eval(ctx):0;
    double n=node.args[1]?node.args[1]->eval(ctx):0;
    double p=std::pow(10.0,std::round(n));
    return std::round(v*p)/p;}
