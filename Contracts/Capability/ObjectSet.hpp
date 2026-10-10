#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace visionai::contracts::capability {

// BoundingBox（目标框）采用左上角 (x1,y1) + 右下角 (x2,y2) 的原图像素坐标。
struct BoundingBox {
    float x1{};
    float y1{};
    float x2{};
    float y2{};
};

// DetectedObject（检测目标）是模型后处理后的标准检测结果。
struct DetectedObject {
    std::uint64_t object_id{};
    int class_id{};
    std::string class_name;
    float confidence{};
    BoundingBox bbox;
};

// ObjectSet（目标集合）对应单个视频帧的全部检测结果。
struct ObjectSet {
    std::uint64_t stream_id{};
    std::uint64_t frame_id{};
    std::int64_t timestamp_ns{};
    std::vector<DetectedObject> objects;
};

} // namespace visionai::contracts::capability
