#pragma once

#include "Event/EventCandidate.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace visionai::edge::event {

// EventEngine（事件引擎）负责候选事件编号、冷却去重和状态边界控制。
//
// 该对象支持多线程并发调用 TryEmit（尝试发布）。
// 内部 key 状态有容量上限，避免摄像机/业务 key 动态增长导致长期内存占用。
class EventEngine {
public:
    explicit EventEngine(std::size_t max_tracked_keys = 4096);

    std::optional<contracts::event::EventCandidate> TryEmit(
        std::string event_key,
        contracts::event::EventCandidate candidate,
        std::int64_t cooldown_ns);

    void Reset(const std::string& event_key);
    void Clear();
    std::size_t TrackedKeyCount() const noexcept;

private:
    void EvictOldestLocked();

    mutable std::mutex mutex_;
    std::uint64_t sequence_{1};
    std::unordered_map<std::string, std::int64_t> last_emit_ns_;
    std::size_t max_tracked_keys_;
};

} // namespace visionai::edge::event
