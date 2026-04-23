#include "formula.h"
#include "sum.h"
#include "avg.h"
#include "min.h"
#include "max.h"
#include "count.h"
#include "if.h"
#include "abs.h"
#include "round.h"
#include "sqrt.h"
#include "power.h"
#include <unordered_map>
#include <functional>
// Dispatch FunctionNode::eval to the appropriate formula function by name
double FunctionNode::eval(EvalCtx&ctx){
    using FnPtr=double(*)(FunctionNode&,EvalCtx&);
    static const std::unordered_map<std::string,FnPtr> fns={
        {"SUM",    fn_SUM},
        {"AVERAGE",fn_AVERAGE},
        {"MIN",    fn_MIN},
        {"MAX",    fn_MAX},
        {"COUNT",  fn_COUNT},
        {"IF",     fn_IF},
        {"ABS",    fn_ABS},
        {"ROUND",  fn_ROUND},
        {"SQRT",   fn_SQRT},
        {"POWER",  fn_POWER},
    };
    auto it=fns.find(name);
    if(it!=fns.end())return it->second(*this,ctx);
    ctx.err=true;return 0;} // unknown function
