#include "power.h"
#include <cmath>
// POWER: raise base to exponent; POWER(base, exponent)
double fn_POWER(FunctionNode&node,EvalCtx&ctx){
    if(node.args.size()<2){ctx.err=true;return 0;}
    double base=node.args[0]?node.args[0]->eval(ctx):0;
    double exp=node.args[1]?node.args[1]->eval(ctx):0;
    return std::pow(base,exp);}
