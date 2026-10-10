#include "Spatial/ZoneMembership.hpp"

#include <algorithm>
#include <cmath>

namespace visionai::algorithms::spatial {

ZoneMembership::ZoneMembership(std::vector<Point2f> polygon)
    : polygon_(std::move(polygon)) {
    // 配置错误不在实时线程抛异常，而是将区域标记为无效并稳定返回 outside（区域外）。
    valid_ =
        polygon_.size() >= 3 &&
        std::all_of(
            polygon_.begin(),
            polygon_.end(),
            [](const Point2f& point) {
                return std::isfinite(point.x) && std::isfinite(point.y);
            });
}

bool ZoneMembership::Contains(Point2f point) const {
    if (!valid_ || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = polygon_.size() - 1; i < polygon_.size(); j = i++) {
        const auto& pi = polygon_[i];
        const auto& pj = polygon_[j];

        // 只有边跨越当前水平射线时才计算交点，因此分母不会为 0。
        const bool crosses =
            ((pi.y > point.y) != (pj.y > point.y)) &&
            (point.x <
             (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x);

        if (crosses) inside = !inside;
    }
    return inside;
}

std::vector<ZoneState> ZoneMembership::Evaluate(
    const contracts::capability::TrackSet& tracks) const {
    std::vector<ZoneState> result;
    result.reserve(tracks.tracks.size());

    for (const auto& track : tracks.tracks) {
        // 采用目标框底边中心作为人体“脚点”，比 bbox 中心更符合地面区域语义。
        const Point2f foot{
            (track.bbox.x1 + track.bbox.x2) * 0.5F,
            track.bbox.y2
        };
        result.push_back({track.track_id, Contains(foot)});
    }
    return result;
}

} // namespace visionai::algorithms::spatial
