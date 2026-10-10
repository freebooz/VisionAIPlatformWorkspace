#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace visionai::algorithms::temporal {

// DurationTracker（持续时间跟踪器）统计某个逻辑 key 的条件连续成立时长。
//
// 稳定性策略：
// - 支持时间戳回退自动重置；
// - 内部状态有容量上限，避免轨迹消失但调用方忘记 Reset 时无限增长；
// - 所有公开方法线程安全。
class DurationTracker {
public:
    explicit DurationTracker(std::size_t max_tracked_keys = 4096);

    double Update(std::uint64_t key, bool condition, std::int64_t timestamp_ns);
    void Reset(std::uint64_t key);
    void ResetAll();
    std::size_t TrackedKeyCount() const noexcept;

private:
    struct State {
        std::int64_t started_at_ns{};
        std::int64_t last_seen_ns{};
    };

    void EvictOldestLocked();

    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, State> states_;
    std::size_t max_tracked_keys_;
};

} // namespace visionai::algorithms::temporal
