#include "EventEngineLite/EventEngine.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace visionai::edge::event {

EventEngine::EventEngine(std::size_t max_tracked_keys)
    : max_tracked_keys_(max_tracked_keys) {
    if (max_tracked_keys_ == 0) {
        throw std::invalid_argument("EventEngine max_tracked_keys 必须大于 0");
    }
}

std::optional<contracts::event::EventCandidate> EventEngine::TryEmit(
    std::string event_key,
    contracts::event::EventCandidate candidate,
    std::int64_t cooldown_ns) {
    if (event_key.empty()) {
        return std::nullopt;
    }

    // 负冷却时间没有业务意义，按 0 处理，避免出现难以解释的去重行为。
    const auto effective_cooldown = std::max<std::int64_t>(0, cooldown_ns);

    std::lock_guard lock(mutex_);

    const auto it = last_emit_ns_.find(event_key);
    if (it != last_emit_ns_.end()) {
        // 只在时间戳没有回退时应用冷却；时间戳回退通常意味着流重连或时间基重置，
        // 此时允许重新发布并更新基线，避免事件永久被旧时间戳压制。
        if (candidate.observed_at_ns >= it->second) {
            const long double elapsed =
                static_cast<long double>(candidate.observed_at_ns) -
                static_cast<long double>(it->second);
            if (elapsed < static_cast<long double>(effective_cooldown)) {
                return std::nullopt;
            }
        }
    } else if (last_emit_ns_.size() >= max_tracked_keys_) {
        EvictOldestLocked();
    }

    // 事件编号包含观测时间和单进程序列号，便于人工审阅和日志关联。
    candidate.event_id =
        "EVT-" +
        std::to_string(candidate.observed_at_ns) +
        "-" +
        std::to_string(sequence_);

    // uint64_t 序列达到上限后从 1 重新开始；结合时间戳后仍能避免现实场景下的编号碰撞。
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        sequence_ = 1;
    } else {
        ++sequence_;
    }

    last_emit_ns_[std::move(event_key)] = candidate.observed_at_ns;
    return candidate;
}

void EventEngine::Reset(const std::string& event_key) {
    std::lock_guard lock(mutex_);
    last_emit_ns_.erase(event_key);
}

void EventEngine::Clear() {
    std::lock_guard lock(mutex_);
    last_emit_ns_.clear();
}

std::size_t EventEngine::TrackedKeyCount() const noexcept {
    std::lock_guard lock(mutex_);
    return last_emit_ns_.size();
}

void EventEngine::EvictOldestLocked() {
    if (last_emit_ns_.empty()) {
        return;
    }

    // 容量达到上限时只做一次 O(N) 淘汰；正常重复事件热路径仍为哈希表 O(1) 查找。
    const auto oldest = std::min_element(
        last_emit_ns_.begin(),
        last_emit_ns_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });
    last_emit_ns_.erase(oldest);
}

} // namespace visionai::edge::event
