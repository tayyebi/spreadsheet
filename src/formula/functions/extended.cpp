#include "extended.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <unordered_map>

namespace {
std::vector<double> valuesFor(FunctionNode& node, EvalCtx& ctx) {
    return node.isRange
        ? collectRange(ctx, node.r1, node.c1, node.r2, node.c2)
        : collectArgs(ctx, node.args);
}

bool requireArgs(FunctionNode& node, EvalCtx& ctx, size_t n) {
    if (node.args.size() < n) { ctx.err = true; return false; }
    return true;
}
}  // namespace

double fn_PRODUCT(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) return 0;
    double out = 1.0;
    for (double x : v) out *= x;
    return out;
}

double fn_QUOTIENT(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double num = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double den = node.args[1] ? node.args[1]->eval(ctx) : 0;
    if (den == 0) { ctx.err = true; return 0; }
    return std::trunc(num / den);
}

double fn_MOD(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double num = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double den = node.args[1] ? node.args[1]->eval(ctx) : 0;
    if (den == 0) { ctx.err = true; return 0; }
    return num - den * std::floor(num / den);
}

double fn_MROUND(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double multiple = node.args[1] ? node.args[1]->eval(ctx) : 0;
    if (multiple == 0) { ctx.err = true; return 0; }
    return std::round(value / multiple) * multiple;
}

double fn_ROUNDUP(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double n = node.args[1] ? node.args[1]->eval(ctx) : 0;
    double p = std::pow(10.0, std::round(n));
    return (value >= 0) ? std::ceil(value * p) / p : std::floor(value * p) / p;
}

double fn_ROUNDDOWN(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double n = node.args[1] ? node.args[1]->eval(ctx) : 0;
    double p = std::pow(10.0, std::round(n));
    return (value >= 0) ? std::floor(value * p) / p : std::ceil(value * p) / p;
}

double fn_CEILING(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double sig = (node.args.size() > 1 && node.args[1]) ? node.args[1]->eval(ctx) : 1.0;
    if (sig == 0) { ctx.err = true; return 0; }
    return std::ceil(value / sig) * sig;
}

double fn_FLOOR(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double sig = (node.args.size() > 1 && node.args[1]) ? node.args[1]->eval(ctx) : 1.0;
    if (sig == 0) { ctx.err = true; return 0; }
    return std::floor(value / sig) * sig;
}

double fn_INT(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    return std::floor(value);
}

double fn_LOG(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double base = (node.args.size() > 1 && node.args[1]) ? node.args[1]->eval(ctx) : 10.0;
    if (value <= 0 || base <= 0 || base == 1) { ctx.err = true; return 0; }
    return std::log(value) / std::log(base);
}

double fn_LOG10(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    if (value <= 0) { ctx.err = true; return 0; }
    return std::log10(value);
}

double fn_LN(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    if (value <= 0) { ctx.err = true; return 0; }
    return std::log(value);
}

double fn_EXP(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    return std::exp(value);
}

double fn_PI(FunctionNode&, EvalCtx&) {
    return std::acos(-1.0);
}

double fn_SIN(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    return std::sin(node.args[0] ? node.args[0]->eval(ctx) : 0);
}

double fn_COS(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    return std::cos(node.args[0] ? node.args[0]->eval(ctx) : 0);
}

double fn_TAN(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    return std::tan(node.args[0] ? node.args[0]->eval(ctx) : 0);
}

double fn_ASIN(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    if (value < -1 || value > 1) { ctx.err = true; return 0; }
    return std::asin(value);
}

double fn_ACOS(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    if (value < -1 || value > 1) { ctx.err = true; return 0; }
    return std::acos(value);
}

double fn_ATAN(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    return std::atan(node.args[0] ? node.args[0]->eval(ctx) : 0);
}

double fn_RADIANS(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    return value * std::acos(-1.0) / 180.0;
}

double fn_DEGREES(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    double value = node.args[0] ? node.args[0]->eval(ctx) : 0;
    return value * 180.0 / std::acos(-1.0);
}

double fn_RAND(FunctionNode&, EvalCtx&) {
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

double fn_RANDBETWEEN(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    double lo = node.args[0] ? node.args[0]->eval(ctx) : 0;
    double hi = node.args[1] ? node.args[1]->eval(ctx) : 0;
    int low = static_cast<int>(std::ceil(std::min(lo, hi)));
    int high = static_cast<int>(std::floor(std::max(lo, hi)));
    if (low > high) { ctx.err = true; return 0; }
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(low, high);
    return static_cast<double>(dist(rng));
}

double fn_AND(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    for (double x : v) if (x == 0) return 0;
    return 1;
}

double fn_OR(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    for (double x : v) if (x != 0) return 1;
    return 0;
}

double fn_XOR(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    bool out = false;
    for (double x : v) out ^= (x != 0);
    return out ? 1.0 : 0.0;
}

double fn_NOT(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.empty()) { ctx.err = true; return 0; }
    return (node.args[0] && node.args[0]->eval(ctx) != 0) ? 0.0 : 1.0;
}

double fn_IFS(FunctionNode& node, EvalCtx& ctx) {
    if (node.args.size() < 2 || (node.args.size() % 2 != 0)) {
        ctx.err = true;
        return 0;
    }
    for (size_t i = 0; i < node.args.size(); i += 2) {
        double cond = node.args[i] ? node.args[i]->eval(ctx) : 0;
        if (ctx.err) return 0;
        if (cond != 0) return node.args[i + 1] ? node.args[i + 1]->eval(ctx) : 0;
    }
    return 0;
}

double fn_MEDIAN(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

double fn_MODE(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    std::unordered_map<std::uint64_t, size_t> freq;
    size_t best = 0;
    double bestVal = v.front();
    for (double x : v) {
        std::uint64_t key = std::bit_cast<std::uint64_t>(x);
        size_t f = ++freq[key];
        if (f > best || (f == best && x < bestVal)) {
            best = f;
            bestVal = x;
        }
    }
    if (best < 2) { ctx.err = true; return 0; }
    return bestVal;
}

double fn_VAR_P(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.empty()) { ctx.err = true; return 0; }
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    double acc = 0.0;
    for (double x : v) {
        double d = x - mean;
        acc += d * d;
    }
    return acc / static_cast<double>(v.size());
}

double fn_VAR_S(FunctionNode& node, EvalCtx& ctx) {
    auto v = valuesFor(node, ctx);
    if (v.size() < 2) { ctx.err = true; return 0; }
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    double acc = 0.0;
    for (double x : v) {
        double d = x - mean;
        acc += d * d;
    }
    return acc / static_cast<double>(v.size() - 1);
}

double fn_STDEV_P(FunctionNode& node, EvalCtx& ctx) {
    return std::sqrt(fn_VAR_P(node, ctx));
}

double fn_STDEV_S(FunctionNode& node, EvalCtx& ctx) {
    return std::sqrt(fn_VAR_S(node, ctx));
}

double fn_LARGE(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    std::vector<double> vals;
    for (size_t i = 0; i + 1 < node.args.size(); ++i)
        if (node.args[i]) vals.push_back(node.args[i]->eval(ctx));
    if (vals.empty()) { ctx.err = true; return 0; }
    double kd = node.args.back() ? node.args.back()->eval(ctx) : 0;
    int k = static_cast<int>(std::round(kd));
    if (k < 1 || k > static_cast<int>(vals.size())) { ctx.err = true; return 0; }
    std::sort(vals.begin(), vals.end(), std::greater<double>());
    return vals[static_cast<size_t>(k - 1)];
}

double fn_SMALL(FunctionNode& node, EvalCtx& ctx) {
    if (!requireArgs(node, ctx, 2)) return 0;
    std::vector<double> vals;
    for (size_t i = 0; i + 1 < node.args.size(); ++i)
        if (node.args[i]) vals.push_back(node.args[i]->eval(ctx));
    if (vals.empty()) { ctx.err = true; return 0; }
    double kd = node.args.back() ? node.args.back()->eval(ctx) : 0;
    int k = static_cast<int>(std::round(kd));
    if (k < 1 || k > static_cast<int>(vals.size())) { ctx.err = true; return 0; }
    std::sort(vals.begin(), vals.end());
    return vals[static_cast<size_t>(k - 1)];
}
