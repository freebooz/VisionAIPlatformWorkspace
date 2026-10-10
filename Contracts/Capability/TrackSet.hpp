#pragma once

#include "Capability/ObjectSet.hpp"
#include <cstdint>
#include <vector>

namespace visionai::contracts::capability {

// Track（目标轨迹）是跨帧稳定目标标识及其最近状态。
// velocity_x / velocity_y 当前表示相邻已匹配帧之间的像素位移，不等同于物理速度。
struct Track {
    std::uint64_t track_id{};
    int class_id{};
    float confidence{};
    BoundingBox bbox;
    float velocity_x{};
    float velocity_y{};
    std::uint32_t age{};
    std::uint32_t missed{};
};

// TrackSet（轨迹集合）对应当前帧之后跟踪器维护的活动轨迹快照。
struct TrackSet {
    std::uint64_t stream_id{};
    std::uint64_t frame_id{};
    std::int64_t timestamp_ns{};
    std::vector<Track> tracks;
};

} // namespace visionai::contracts::capability
