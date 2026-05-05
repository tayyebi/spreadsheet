#pragma once
#include "function_base.h"

double fn_PRODUCT(FunctionNode& node, EvalCtx& ctx);
double fn_QUOTIENT(FunctionNode& node, EvalCtx& ctx);
double fn_MOD(FunctionNode& node, EvalCtx& ctx);
double fn_MROUND(FunctionNode& node, EvalCtx& ctx);
double fn_ROUNDUP(FunctionNode& node, EvalCtx& ctx);
double fn_ROUNDDOWN(FunctionNode& node, EvalCtx& ctx);
double fn_CEILING(FunctionNode& node, EvalCtx& ctx);
double fn_FLOOR(FunctionNode& node, EvalCtx& ctx);
double fn_INT(FunctionNode& node, EvalCtx& ctx);
double fn_LOG(FunctionNode& node, EvalCtx& ctx);
double fn_LOG10(FunctionNode& node, EvalCtx& ctx);
double fn_LN(FunctionNode& node, EvalCtx& ctx);
double fn_EXP(FunctionNode& node, EvalCtx& ctx);
double fn_PI(FunctionNode& node, EvalCtx& ctx);
double fn_SIN(FunctionNode& node, EvalCtx& ctx);
double fn_COS(FunctionNode& node, EvalCtx& ctx);
double fn_TAN(FunctionNode& node, EvalCtx& ctx);
double fn_ASIN(FunctionNode& node, EvalCtx& ctx);
double fn_ACOS(FunctionNode& node, EvalCtx& ctx);
double fn_ATAN(FunctionNode& node, EvalCtx& ctx);
double fn_RADIANS(FunctionNode& node, EvalCtx& ctx);
double fn_DEGREES(FunctionNode& node, EvalCtx& ctx);
double fn_RAND(FunctionNode& node, EvalCtx& ctx);
double fn_RANDBETWEEN(FunctionNode& node, EvalCtx& ctx);

double fn_AND(FunctionNode& node, EvalCtx& ctx);
double fn_OR(FunctionNode& node, EvalCtx& ctx);
double fn_XOR(FunctionNode& node, EvalCtx& ctx);
double fn_NOT(FunctionNode& node, EvalCtx& ctx);
double fn_IFS(FunctionNode& node, EvalCtx& ctx);

double fn_MEDIAN(FunctionNode& node, EvalCtx& ctx);
double fn_MODE(FunctionNode& node, EvalCtx& ctx);
double fn_VAR_P(FunctionNode& node, EvalCtx& ctx);
double fn_VAR_S(FunctionNode& node, EvalCtx& ctx);
double fn_STDEV_P(FunctionNode& node, EvalCtx& ctx);
double fn_STDEV_S(FunctionNode& node, EvalCtx& ctx);
double fn_LARGE(FunctionNode& node, EvalCtx& ctx);
double fn_SMALL(FunctionNode& node, EvalCtx& ctx);
