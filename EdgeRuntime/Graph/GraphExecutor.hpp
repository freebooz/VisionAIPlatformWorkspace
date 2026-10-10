#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace visionai::edge::graph {

// GraphExecutor（DAG 算法图执行器）负责按依赖关系执行算法节点。
//
// 生命周期：
// 1. 构建阶段调用 AddNode（添加节点）；
// 2. 首次 Execute（执行）时编译并冻结拓扑顺序；
// 3. 运行阶段允许多个线程并发 Execute，但冻结后不再允许增加节点。
//
// 性能：
// - 拓扑排序只在首次执行时计算一次；
// - 后续执行由原来的近似 O(V²) 降为 O(V)，其中 V 为节点数；
// - 编译后的执行计划使用 shared_ptr（共享指针）快照，执行回调时不持有图结构互斥锁。
class GraphExecutor {
public:
    using NodeFn = std::function<bool()>;

    // AddNode（添加节点）仅允许在图冻结前调用。
    bool AddNode(std::string id, std::vector<std::string> dependencies, NodeFn fn);

    // Validate（验证）检查依赖存在性、重复依赖和循环依赖，但不会冻结执行图。
    bool Validate(std::string* error = nullptr) const;

    // Execute（执行）首次调用会编译并冻结 DAG；节点异常会被转换为错误信息，不越过执行器边界传播。
    bool Execute(std::string* error = nullptr) const;

private:
    struct Node {
        std::string id;
        std::vector<std::string> dependencies;
        std::shared_ptr<const NodeFn> fn;
    };

    struct ExecutionStep {
        std::string id;
        std::shared_ptr<const NodeFn> fn;
    };

    // BuildExecutionPlanLocked（构建执行计划）要求调用方已经持有 mutex_。
    bool BuildExecutionPlanLocked(
        std::vector<ExecutionStep>& plan,
        std::string* error) const;

    // mutex_ 只保护图构建和首次编译，不覆盖实际算法回调执行。
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Node> nodes_;
    mutable std::shared_ptr<const std::vector<ExecutionStep>> compiled_plan_;
};

} // namespace visionai::edge::graph
