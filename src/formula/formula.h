#pragma once // guard
#include <string> // string
#include <memory> // unique_ptr
#include <vector> // vector
#include <variant> // variant
#include <set> // set
#include <cstdint> // uint64_t
class Spreadsheet; // fwd
using FormulaResult=std::variant<double,std::string>; // result
struct EvalCtx; // fwd
struct ASTNode{virtual~ASTNode()=default;virtual double eval(EvalCtx&)=0;}; // base
struct NumberNode:ASTNode{double val=0;double eval(EvalCtx&)override;}; // num
struct CellRefNode:ASTNode{int row=0,col=0;double eval(EvalCtx&)override;}; // ref
struct BinaryOpNode:ASTNode{char op='+';std::unique_ptr<ASTNode>left,right;double eval(EvalCtx&)override;}; // op
struct FunctionNode:ASTNode{std::string name;bool isRange=false;int r1=0,c1=0,r2=0,c2=0; // func
    std::vector<std::unique_ptr<ASTNode>>args;double eval(EvalCtx&)override;};
struct EvalCtx{Spreadsheet&sheet;std::set<uint64_t>&vis,&done;bool err=false,cyclic=false;}; // ctx
FormulaResult evaluateFormula(const std::string&e,Spreadsheet&s,std::set<uint64_t>&vis,std::set<uint64_t>&done); // eval
