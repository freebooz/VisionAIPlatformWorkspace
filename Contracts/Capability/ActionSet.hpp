#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace visionai::contracts::capability {

// ActionScore（动作分数）描述一个动作类别及其置信度。
struct ActionScore {
    std::string label;
    float confidence{};
};

// ActionInstance（单轨迹动作结果）允许同一轨迹保留多个候选动作分数。
struct ActionInstance {
    std::uint64_t track_id{};
    std::vector<ActionScore> scores;
};

// ActionSet（动作集合）对应单帧或一个时序窗口的标准动作识别输出。
struct ActionSet {
    std::uint64_t stream_id{};
    std::uint64_t frame_id{};
    std::int64_t timestamp_ns{};
    std::vector<ActionInstance> actions;
};

} // namespace visionai::contracts::capability
