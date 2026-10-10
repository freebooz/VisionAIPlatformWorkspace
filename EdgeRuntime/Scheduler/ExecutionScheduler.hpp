#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace visionai::edge::scheduler {

// ExecutionScheduler（执行调度器）按逻辑 key（算法/流）限制目标执行帧率。
//
// 稳定性约束：
// - 内部状态有最大容量，避免动态 key 无限增长形成长期内存占用；
// - 时间戳回退（例如视频重连后 PTS 重置）会自动重置该 key 的调度基线；
// - NaN / Inf（非数/无穷）FPS 会被拒绝，避免浮点转整数产生未定义或平台差异行为。
class ExecutionScheduler {
public:
    explicit ExecutionScheduler(std::size_t max_states = 4096);

    bool ShouldRun(const std::string& key, std::int64_t timestamp_ns, double target_fps);
    void Reset(const std::string& key);
    void ResetAll();
    std::size_t StateCount() const noexcept;

private:
    struct State {
        std::int64_t next_run_ns{};
        std::int64_t last_timestamp_ns{};
    };

    void EvictOldestLocked();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, State> states_;
    std::size_t max_states_;
};

} // namespace visionai::edge::scheduler
