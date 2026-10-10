#include "Temporal/DurationTracker.hpp"

#include <algorithm>
#include <stdexcept>

namespace visionai::algorithms::temporal {

DurationTracker::DurationTracker(std::size_t max_tracked_keys)
    : max_tracked_keys_(max_tracked_keys) {
    if (max_tracked_keys_ == 0) {
        throw std::invalid_argument("DurationTracker max_tracked_keys 必须大于 0");
    }
}

double DurationTracker::Update(
    std::uint64_t key,
    bool condition,
    std::int64_t timestamp_ns) {
    std::lock_guard lock(mutex_);

    if (!condition) {
        states_.erase(key);
        return 0.0;
    }

    auto it = states_.find(key);
    if (it == states_.end()) {
        if (states_.size() >= max_tracked_keys_) {
            EvictOldestLocked();
        }

        it = states_.emplace(
            key,
            State{
                .started_at_ns = timestamp_ns,
                .last_seen_ns = timestamp_ns
            }).first;
        return 0.0;
    }

    auto& state = it->second;

    // PTS（显示时间戳）回退时重新开始计时，避免返回负持续时间。
    if (timestamp_ns < state.last_seen_ns) {
        state.started_at_ns = timestamp_ns;
        state.last_seen_ns = timestamp_ns;
        return 0.0;
    }

    state.last_seen_ns = timestamp_ns;
    const long double elapsed_ns =
        static_cast<long double>(timestamp_ns) -
        static_cast<long double>(state.started_at_ns);
    return static_cast<double>(elapsed_ns / 1'000'000'000.0L);
}

void DurationTracker::Reset(std::uint64_t key) {
    std::lock_guard lock(mutex_);
    states_.erase(key);
}

void DurationTracker::ResetAll() {
    std::lock_guard lock(mutex_);
    states_.clear();
}

std::size_t DurationTracker::TrackedKeyCount() const noexcept {
    std::lock_guard lock(mutex_);
    return states_.size();
}

void DurationTracker::EvictOldestLocked() {
    if (states_.empty()) {
        return;
    }

    // 只在容量满且新增 key 时执行 O(N) 淘汰，不进入普通 Update 热路径。
    const auto oldest = std::min_element(
        states_.begin(),
        states_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second.last_seen_ns < rhs.second.last_seen_ns;
        });
    states_.erase(oldest);
}

} // namespace visionai::algorithms::temporal
