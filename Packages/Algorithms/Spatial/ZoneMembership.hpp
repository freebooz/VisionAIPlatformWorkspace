#pragma once

#include "Capability/TrackSet.hpp"
#include <cstdint>
#include <vector>

namespace visionai::algorithms::spatial {

// Point2f（二维点）使用视频原图坐标。
struct Point2f {
    float x{};
    float y{};
};

// ZoneState（区域状态）表示某条轨迹当前是否位于区域内部。
struct ZoneState {
    std::uint64_t track_id{};
    bool inside{};
};

// ZoneMembership（区域归属判断）使用 Point-in-Polygon（点在多边形内）算法判断人体脚点位置。
// 构造后对象只读，因此可安全被多个线程并发 Evaluate（评估）。
class ZoneMembership {
public:
    explicit ZoneMembership(std::vector<Point2f> polygon);
    std::vector<ZoneState> Evaluate(
        const contracts::capability::TrackSet& tracks) const;

private:
    bool Contains(Point2f point) const;
    std::vector<Point2f> polygon_;
    bool valid_{false};
};

} // namespace visionai::algorithms::spatial
