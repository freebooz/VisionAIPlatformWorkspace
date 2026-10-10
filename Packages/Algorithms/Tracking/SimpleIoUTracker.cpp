#include "Tracking/SimpleIoUTracker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace visionai::algorithms::tracking {

SimpleIoUTracker::SimpleIoUTracker(float iou_threshold, std::uint32_t max_missed)
    : iou_threshold_(std::clamp(iou_threshold, 0.0F, 1.0F)),
      max_missed_(max_missed) {}

float SimpleIoUTracker::IoU(
    const contracts::capability::BoundingBox& a,
    const contracts::capability::BoundingBox& b) {
    const float x1 = std::max(a.x1, b.x1);
    const float y1 = std::max(a.y1, b.y1);
    const float x2 = std::min(a.x2, b.x2);
    const float y2 = std::min(a.y2, b.y2);
    const float iw = std::max(0.0F, x2 - x1);
    const float ih = std::max(0.0F, y2 - y1);
    const float intersection = iw * ih;
    const float area_a = std::max(0.0F, a.x2 - a.x1) * std::max(0.0F, a.y2 - a.y1);
    const float area_b = std::max(0.0F, b.x2 - b.x1) * std::max(0.0F, b.y2 - b.y1);
    const float denominator = area_a + area_b - intersection;
    return denominator > 0.0F ? intersection / denominator : 0.0F;
}

bool SimpleIoUTracker::IsValidDetection(
    const contracts::capability::DetectedObject& object) {
    const auto& box = object.bbox;

    // NaN / Inf 坐标会污染 IoU 计算并导致轨迹持续分裂，进入跟踪器前直接过滤。
    if (!std::isfinite(box.x1) ||
        !std::isfinite(box.y1) ||
        !std::isfinite(box.x2) ||
        !std::isfinite(box.y2) ||
        !std::isfinite(object.confidence)) {
        return false;
    }

    return box.x2 > box.x1 && box.y2 > box.y1;
}

void SimpleIoUTracker::SaturatingIncrement(std::uint32_t& value) noexcept {
    if (value != std::numeric_limits<std::uint32_t>::max()) {
        ++value;
    }
}

contracts::capability::TrackSet SimpleIoUTracker::Update(
    const contracts::capability::ObjectSet& detections) {
    std::lock_guard lock(mutex_);

    // valid_indices（有效检测索引）避免复制 DetectedObject（检测目标）中的字符串等数据。
    std::vector<std::size_t> valid_indices;
    valid_indices.reserve(detections.objects.size());
    for (std::size_t i = 0; i < detections.objects.size(); ++i) {
        if (IsValidDetection(detections.objects[i])) {
            valid_indices.push_back(i);
        }
    }

    std::vector<bool> object_used(detections.objects.size(), false);

    for (auto& track : tracks_) {
        float best_iou = 0.0F;
        std::size_t best_index = detections.objects.size();

        for (const auto i : valid_indices) {
            if (object_used[i] || detections.objects[i].class_id != track.class_id) continue;
            const auto score = IoU(track.bbox, detections.objects[i].bbox);
            if (score > best_iou) {
                best_iou = score;
                best_index = i;
            }
        }

        if (best_index < detections.objects.size() && best_iou >= iou_threshold_) {
            const auto& object = detections.objects[best_index];
            const float old_cx = (track.bbox.x1 + track.bbox.x2) * 0.5F;
            const float old_cy = (track.bbox.y1 + track.bbox.y2) * 0.5F;
            const float new_cx = (object.bbox.x1 + object.bbox.x2) * 0.5F;
            const float new_cy = (object.bbox.y1 + object.bbox.y2) * 0.5F;
            track.velocity_x = new_cx - old_cx;
            track.velocity_y = new_cy - old_cy;
            track.bbox = object.bbox;
            track.confidence = object.confidence;
            track.missed = 0;
            SaturatingIncrement(track.age);
            object_used[best_index] = true;
        } else {
            SaturatingIncrement(track.missed);
            SaturatingIncrement(track.age);
        }
    }

    // 清理超过 max_missed（最大丢失帧数）的轨迹，保证活动轨迹集合不会因为历史目标无限增长。
    tracks_.erase(
        std::remove_if(
            tracks_.begin(),
            tracks_.end(),
            [this](const auto& track) { return track.missed > max_missed_; }),
        tracks_.end());

    // 最多新增 valid_indices.size() 条轨迹，提前 reserve 减少 vector 扩容次数。
    tracks_.reserve(tracks_.size() + valid_indices.size());
    for (const auto i : valid_indices) {
        if (object_used[i]) continue;
        const auto& object = detections.objects[i];
        tracks_.push_back({
            next_track_id_++,
            object.class_id,
            object.confidence,
            object.bbox,
            0.0F,
            0.0F,
            1,
            0
        });
    }

    // TrackSet（轨迹集合）需要拥有独立快照，因此这里会复制活动轨迹 vector；
    // 相比复制视频像素，该对象规模很小，并能避免调用方观察到内部状态后续被修改。
    return {
        detections.stream_id,
        detections.frame_id,
        detections.timestamp_ns,
        tracks_
    };
}

void SimpleIoUTracker::Reset() {
    std::lock_guard lock(mutex_);
    tracks_.clear();
    next_track_id_ = 1;
}

std::size_t SimpleIoUTracker::ActiveTrackCount() const noexcept {
    std::lock_guard lock(mutex_);
    return tracks_.size();
}

} // namespace visionai::algorithms::tracking
