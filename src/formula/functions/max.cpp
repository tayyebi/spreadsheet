#include "max.h"
#include <limits>
// MAX: largest numeric value in range or argument list
double fn_MAX(FunctionNode&node,EvalCtx&ctx){
    auto v=node.isRange?collectRange(ctx,node.r1,node.c1,node.r2,node.c2)
                       :collectArgs(ctx,node.args);
    if(v.empty()){ctx.err=true;return 0;}
    double m=std::numeric_limits<double>::lowest();for(double x:v)if(x>m)m=x;return m;}
