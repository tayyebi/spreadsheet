#include "count.h"
// COUNT: number of numeric cells in range or argument list
double fn_COUNT(FunctionNode&node,EvalCtx&ctx){
    auto v=node.isRange?collectRange(ctx,node.r1,node.c1,node.r2,node.c2)
                       :collectArgs(ctx,node.args);
    return static_cast<double>(v.size());}
