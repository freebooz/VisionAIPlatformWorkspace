#include "RuleEngineLite/RuleEngine.hpp"

#include <cmath>

namespace visionai::edge::rule {

bool RuleEngine::Compare(double actual, ComparisonOp op, double expected) {
    // NaN / Inf（非数/无穷）通常代表上游计算异常，任何业务比较都按失败处理，
    // 避免异常数值意外穿透规则触发告警。
    if (!std::isfinite(actual) || !std::isfinite(expected)) {
        return false;
    }

    switch (op) {
        case ComparisonOp::Eq: return std::fabs(actual - expected) < 1e-9;
        case ComparisonOp::Ne: return std::fabs(actual - expected) >= 1e-9;
        case ComparisonOp::Lt: return actual < expected;
        case ComparisonOp::Le: return actual <= expected;
        case ComparisonOp::Gt: return actual > expected;
        case ComparisonOp::Ge: return actual >= expected;
    }
    return false;
}

bool RuleEngine::EvaluateAll(
    const std::vector<Condition>& conditions,
    const std::unordered_map<std::string, double>& facts) const {
    // All（全部满足）语义：任一字段缺失或比较失败即整体失败。
    for (const auto& condition : conditions) {
        const auto it = facts.find(condition.field);
        if (it == facts.end() || !Compare(it->second, condition.op, condition.value)) {
            return false;
        }
    }
    return true;
}

} // namespace visionai::edge::rule
