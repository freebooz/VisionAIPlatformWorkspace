#pragma once

#include "Media/FrameRef.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace visionai::runtime {

// TensorDataType（张量数据类型）定义运行时边界上的基础数据类型。
enum class TensorDataType : std::uint8_t {
    Float32 = 0,
    Float16,
    Int8,
    Int32,
    Int64
};

// TensorView（张量视图）描述上游提供的张量缓冲区。
//
// owner（生命周期持有者）是可选的共享所有权锚点：
// - 同步推理可以只使用 data/native_handle；
// - 异步推理必须复制 TensorView 并保留 owner，直到推理完成回调结束，
//   从而避免 data 或设备句柄在异步任务期间提前释放形成 Use-After-Free（释放后使用）。
struct TensorView {
    void* data{nullptr};
    std::size_t bytes{};
    TensorDataType data_type{TensorDataType::Float32};
    std::vector<std::int64_t> shape;
    contracts::media::MemoryType memory_type{contracts::media::MemoryType::Host};
    int device_id{-1};
    std::uintptr_t native_handle{};
    std::shared_ptr<void> owner;
};

} // namespace visionai::runtime

