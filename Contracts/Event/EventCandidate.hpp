#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace visionai::contracts::event {

// EventCandidate（候选事件）是算法/规则层输出、尚待去重和正式发布的标准事件。
// metrics（指标）只存放可解释的数值证据，避免把大型图像/张量塞入事件对象造成内存膨胀。
struct EventCandidate {
    std::string event_id;
    std::string event_type;
    std::string source_algorithm;
    std::uint64_t stream_id{};
    std::int64_t started_at_ns{};
    std::int64_t observed_at_ns{};
    float confidence{};
    std::unordered_map<std::string, double> metrics;
};

} // namespace visionai::contracts::event
