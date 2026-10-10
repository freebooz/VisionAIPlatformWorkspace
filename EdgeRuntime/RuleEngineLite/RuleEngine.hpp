#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace visionai::edge::rule {

// ComparisonOp（比较操作符）是轻量规则引擎当前支持的基础数值运算。
enum class ComparisonOp { Eq, Ne, Lt, Le, Gt, Ge };

// Condition（规则条件）表示“事实字段 + 比较操作 + 期望值”。
struct Condition {
    std::string field;
    ComparisonOp op{ComparisonOp::Eq};
    double value{};
};

// RuleEngine（轻量规则引擎）当前为无状态对象，可安全并发复用。
class RuleEngine {
public:
    bool EvaluateAll(
        const std::vector<Condition>& conditions,
        const std::unordered_map<std::string, double>& facts) const;

private:
    static bool Compare(double actual, ComparisonOp op, double expected);
};

} // namespace visionai::edge::rule
