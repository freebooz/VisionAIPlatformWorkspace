#pragma once

#include <cstdint>
#include <vector>

namespace visionai::contracts::capability {

// Keypoint2f（二维关键点）使用原图坐标，confidence（置信度）建议归一化到 [0,1]。
struct Keypoint2f {
    float x{};
    float y{};
    float confidence{};
};

// PoseInstance（单人姿态）通过 track_id（轨迹编号）与 Tracking（目标跟踪）结果关联。
struct PoseInstance {
    std::uint64_t track_id{};
    std::vector<Keypoint2f> keypoints;
    float quality{};
};

// PoseSet（姿态集合）保存单帧全部人体姿态结果。
struct PoseSet {
    std::uint64_t stream_id{};
    std::uint64_t frame_id{};
    std::int64_t timestamp_ns{};
    std::vector<PoseInstance> poses;
};

} // namespace visionai::contracts::capability
