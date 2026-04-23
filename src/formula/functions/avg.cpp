#include "avg.h"
// AVERAGE: arithmetic mean of all values
double fn_AVERAGE(FunctionNode&node,EvalCtx&ctx){
    auto v=node.isRange?collectRange(ctx,node.r1,node.c1,node.r2,node.c2)
                       :collectArgs(ctx,node.args);
    if(v.empty()){ctx.err=true;return 0;}
    double t=0;for(double x:v)t+=x;return t/static_cast<double>(v.size());}
