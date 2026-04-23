#include "min.h"
#include <limits>
// MIN: smallest numeric value in range or argument list
double fn_MIN(FunctionNode&node,EvalCtx&ctx){
    auto v=node.isRange?collectRange(ctx,node.r1,node.c1,node.r2,node.c2)
                       :collectArgs(ctx,node.args);
    if(v.empty()){ctx.err=true;return 0;}
    double m=std::numeric_limits<double>::max();for(double x:v)if(x<m)m=x;return m;}
