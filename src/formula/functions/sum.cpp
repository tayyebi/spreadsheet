#include "sum.h"
// SUM: add all values in range or argument list
double fn_SUM(FunctionNode&node,EvalCtx&ctx){
    auto v=node.isRange?collectRange(ctx,node.r1,node.c1,node.r2,node.c2)
                       :collectArgs(ctx,node.args);
    double t=0;for(double x:v)t+=x;return t;}
