#pragma once

#include "Capability/ObjectSet.hpp"
#include "Capability/TrackSet.hpp"
#include <cstdint>
#include <mutex>
#include <vector>

namespace visionai::algorithms::tracking {

// SimpleIoUTracker（基础 IoU 目标跟踪器）用于工程基线和低密度场景。
//
// 性能边界：
// - 当前匹配复杂度约为 O(T×D)，T 为活动轨迹数，D 为检测目标数；
// - 高密度多人场景应替换为 ByteTrack / Hungarian（匈牙利匹配）等更完整实现；
// - 本类仍保持统一 TrackSet（轨迹集合）契约，替换算法不会影响上层 Pipeline（算法管线）。
//
// 线程安全：
// - Update / Reset 使用内部互斥锁串行化，因为同一跟踪器的轨迹状态天然具有时序依赖。
class SimpleIoUTracker {
public:
    explicit SimpleIoUTracker(float iou_threshold = 0.3F, std::uint32_t max_missed = 10);

    contracts::capability::TrackSet Update(
        const contracts::capability::ObjectSet& detections);
    void Reset();
    std::size_t ActiveTrackCount() const noexcept;

private:
    static float IoU(
        const contracts::capability::BoundingBox& a,
        const contracts::capability::BoundingBox& b);
    static bool IsValidDetection(const contracts::capability::DetectedObject& object);
    static void SaturatingIncrement(std::uint32_t& value) noexcept;

    mutable std::mutex mutex_;
    float iou_threshold_;
    std::uint32_t max_missed_;
    std::uint64_t next_track_id_{1};
    std::vector<contracts::capability::Track> tracks_;
};

} // namespace visionai::algorithms::tracking
