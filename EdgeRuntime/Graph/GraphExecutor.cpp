#include "Graph/GraphExecutor.hpp"

#include <exception>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace visionai::edge::graph {

bool GraphExecutor::AddNode(
    std::string id,
    std::vector<std::string> dependencies,
    NodeFn fn) {
    if (id.empty() || !fn) {
        return false;
    }

    std::lock_guard lock(mutex_);

    // 首次 Execute（执行）后执行计划已经冻结。
    // 运行期动态改图会造成不可重复行为和跨线程数据竞争，因此直接拒绝。
    if (compiled_plan_ || nodes_.contains(id)) {
        return false;
    }

    // 同一节点重复声明相同依赖通常是配置错误。
    std::unordered_set<std::string> unique_dependencies;
    unique_dependencies.reserve(dependencies.size());
    for (const auto& dependency : dependencies) {
        if (dependency.empty() || !unique_dependencies.insert(dependency).second) {
            return false;
        }
    }

    // 先构造 Node，再以未移动的 id 作为 unordered_map（无序映射）键。
    // 不能在同一个 emplace 参数列表里一边把 id 当 key、一边 std::move(id)，
    // 因为参数求值顺序不保证“先 key 后 move”，可能把空字符串意外作为 map key。
    Node node{
        id,
        std::move(dependencies),
        std::make_shared<const NodeFn>(std::move(fn))
    };

    const auto [_, inserted] = nodes_.emplace(id, std::move(node));
    return inserted;
}

bool GraphExecutor::BuildExecutionPlanLocked(
    std::vector<ExecutionStep>& plan,
    std::string* error) const {
    std::unordered_map<std::string, int> indegree;
    std::unordered_map<std::string, std::vector<std::string>> outgoing;
    indegree.reserve(nodes_.size());
    outgoing.reserve(nodes_.size());

    for (const auto& [id, node] : nodes_) {
        indegree[id] = static_cast<int>(node.dependencies.size());

        for (const auto& dependency : node.dependencies) {
            if (!nodes_.contains(dependency)) {
                if (error) {
                    *error = "算法图依赖不存在: " + dependency;
                }
                return false;
            }
            outgoing[dependency].push_back(id);
        }
    }

    // 使用按节点 id 排序的优先队列，避免 unordered_map（无序映射）迭代顺序导致执行顺序漂移。
    std::priority_queue<
        std::string,
        std::vector<std::string>,
        std::greater<>> ready;

    for (const auto& [id, degree] : indegree) {
        if (degree == 0) {
            ready.push(id);
        }
    }

    plan.clear();
    plan.reserve(nodes_.size());

    while (!ready.empty()) {
        auto id = ready.top();
        ready.pop();

        const auto node_it = nodes_.find(id);
        if (node_it == nodes_.end()) {
            if (error) {
                *error = "算法图内部状态异常: " + id;
            }
            return false;
        }

        plan.push_back(ExecutionStep{id, node_it->second.fn});

        for (const auto& next : outgoing[id]) {
            auto degree_it = indegree.find(next);
            if (degree_it == indegree.end()) {
                if (error) {
                    *error = "算法图入度状态异常: " + next;
                }
                return false;
            }

            --degree_it->second;
            if (degree_it->second == 0) {
                ready.push(next);
            }
        }
    }

    if (plan.size() != nodes_.size()) {
        if (error) {
            *error = "算法图存在循环依赖";
        }
        return false;
    }

    return true;
}

bool GraphExecutor::Validate(std::string* error) const {
    std::lock_guard lock(mutex_);
    std::vector<ExecutionStep> plan;
    return BuildExecutionPlanLocked(plan, error);
}

bool GraphExecutor::Execute(std::string* error) const {
    std::shared_ptr<const std::vector<ExecutionStep>> plan;

    {
        std::lock_guard lock(mutex_);

        if (!compiled_plan_) {
            auto mutable_plan = std::make_shared<std::vector<ExecutionStep>>();
            if (!BuildExecutionPlanLocked(*mutable_plan, error)) {
                return false;
            }

            // 编译成功后冻结执行计划；后续 Execute 只读取该不可变快照。
            compiled_plan_ =
                std::const_pointer_cast<const std::vector<ExecutionStep>>(mutable_plan);
        }

        plan = compiled_plan_;
    }

    // 执行插件/算法回调时不持有图结构锁，避免慢节点阻塞其他并发执行线程。
    for (const auto& step : *plan) {
        try {
            if (!(*step.fn)()) {
                if (error) {
                    *error = "算法节点执行失败: " + step.id;
                }
                return false;
            }
        } catch (const std::exception& ex) {
            if (error) {
                *error = "算法节点抛出异常: " + step.id + ", " + ex.what();
            }
            return false;
        } catch (...) {
            if (error) {
                *error = "算法节点抛出未知异常: " + step.id;
            }
            return false;
        }
    }

    return true;
}

} // namespace visionai::edge::graph
