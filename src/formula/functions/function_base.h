#pragma once
#include "formula.h"
#include "core.h"
#include <vector>
// Shared helpers for formula function implementations
std::vector<double> collectRange(EvalCtx&ctx,int r1,int c1,int r2,int c2); // collect range values
std::vector<double> collectArgs(EvalCtx&ctx,const std::vector<std::unique_ptr<ASTNode>>&args); // collect arg values
