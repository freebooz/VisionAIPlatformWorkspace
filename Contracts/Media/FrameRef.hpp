#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace visionai::contracts::media {

// PixelFormat（像素格式）描述共享视频帧的像素排列。
// Unknown（未知）只允许用于尚未完成媒体协商的临时状态，进入算法链前必须转换为确定格式。
enum class PixelFormat : std::uint8_t {
    Unknown = 0,
    NV12,
    RGB24,
    BGR24,
    Gray8
};

// MemoryType（内存类型）描述 FrameRef（视频帧引用）指向的底层存储位置。
// 算法不得根据枚举值直接调用厂商 SDK，而应通过对应 Runtime / Media Port（运行时 / 媒体端口）访问。
enum class MemoryType : std::uint8_t {
    Host = 0,
    SharedMemory,
    DmaBuf,
    Cuda,
    Npu
};

// FrameRef（视频帧引用）是跨算法共享视频帧的非拥有型视图。
//
// 生命周期约束：
// 1. Host（主机内存）场景下，data 指向有效像素地址，owner 必须保持底层缓冲区存活。
// 2. CUDA / DMA-BUF / NPU 等设备内存可以主要依赖 native_handle（原生句柄），data 可为空。
// 3. 任何消费者若需要异步使用帧，必须复制 FrameRef 本身以延长 owner 的引用计数，
//    禁止只缓存 data 裸指针后跨越当前调用栈使用。
struct FrameRef {
    std::uint64_t stream_id{};
    std::uint64_t frame_id{};
    std::int64_t timestamp_ns{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride{};
    PixelFormat pixel_format{PixelFormat::Unknown};
    MemoryType memory_type{MemoryType::Host};
    int device_id{-1};
    void* data{nullptr};
    std::size_t bytes{};
    std::uintptr_t native_handle{};
    std::shared_ptr<void> owner;
};

} // namespace visionai::contracts::media
