#include "Scheduler/ExecutionScheduler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace visionai::edge::scheduler {

ExecutionScheduler::ExecutionScheduler(std::size_t max_states)
    : max_states_(max_states) {
    if (max_states_ == 0) {
        throw std::invalid_argument("ExecutionScheduler max_states 必须大于 0");
    }
}

bool ExecutionScheduler::ShouldRun(
    const std::string& key,
    std::int64_t timestamp_ns,
    double target_fps) {
    // <= 0 保留原语义：不限制 FPS，每次调用都执行且不占用状态表。
    if (target_fps <= 0.0) {
        return true;
    }

    // NaN / Inf 不属于合法配置；直接拒绝本次运行，避免 llround（四舍五入）行为不确定。
    if (!std::isfinite(target_fps) || key.empty()) {
        return false;
    }

    const long double raw_interval =
        1'000'000'000.0L / static_cast<long double>(target_fps);

    // 最短间隔为 1ns，最长间隔限制到 int64_t 可表达范围。
    const auto max_interval =
        static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    const auto bounded_interval =
        std::clamp(raw_interval, 1.0L, max_interval);
    const auto interval_ns =
        static_cast<std::int64_t>(std::llround(bounded_interval));

    std::lock_guard lock(mutex_);

    auto it = states_.find(key);
    if (it == states_.end()) {
        if (states_.size() >= max_states_) {
            EvictOldestLocked();
        }
        it = states_.emplace(key, State{timestamp_ns, timestamp_ns}).first;
    }

    auto& state = it->second;

    // 视频重连或源时间基变化可能导致时间戳回退，必须立即重置，否则该算法可能长时间被错误跳过。
    if (timestamp_ns < state.last_timestamp_ns) {
        state.next_run_ns = timestamp_ns;
    }
    state.last_timestamp_ns = timestamp_ns;

    if (timestamp_ns >= state.next_run_ns) {
        // 饱和加法避免接近 int64 上界时发生有符号整数溢出。
        if (timestamp_ns > std::numeric_limits<std::int64_t>::max() - interval_ns) {
            state.next_run_ns = std::numeric_limits<std::int64_t>::max();
        } else {
            state.next_run_ns = timestamp_ns + interval_ns;
        }
        return true;
    }

    return false;
}

void ExecutionScheduler::Reset(const std::string& key) {
    std::lock_guard lock(mutex_);
    states_.erase(key);
}

void ExecutionScheduler::ResetAll() {
    std::lock_guard lock(mutex_);
    states_.clear();
}

std::size_t ExecutionScheduler::StateCount() const noexcept {
    std::lock_guard lock(mutex_);
    return states_.size();
}

void ExecutionScheduler::EvictOldestLocked() {
    if (states_.empty()) {
        return;
    }

    // 容量满时淘汰最久未看到时间戳的 key；仅在新增 key 且达到上限时发生，非逐帧热路径。
    const auto oldest = std::min_element(
        states_.begin(),
        states_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second.last_timestamp_ns < rhs.second.last_timestamp_ns;
        });
    states_.erase(oldest);
}

} // namespace visionai::edge::scheduler
