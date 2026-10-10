#pragma once

#include <cstddef>
#include <memory>

namespace visionai::edge::memory {

// SharedBuffer（共享缓冲租约）把 data（裸地址）与 owner（生命周期持有者）绑定。
// 复制 SharedBuffer / FrameRef 时只增加引用计数，不复制底层像素内存。
struct SharedBuffer {
    std::shared_ptr<void> owner;
    std::byte* data{};
    std::size_t bytes{};
    explicit operator bool() const noexcept { return data != nullptr; }
};

// BufferPool（固定块缓冲池）用于视频帧和高频临时缓冲复用。
//
// 设计要点：
// - 构造阶段一次性分配所有数据块，运行阶段不再申请大块像素内存；
// - Acquire（获取）线程安全；
// - 最后一个 owner 释放时自动把块归还池中；
// - 即使 BufferPool 对象先销毁，只要租约仍存在，内部 State（状态）和数据块仍保持有效。
class BufferPool {
public:
    BufferPool(std::size_t block_size, std::size_t block_count);
    SharedBuffer Acquire();
    std::size_t BlockSize() const noexcept;
    std::size_t Capacity() const noexcept;
    std::size_t Available() const noexcept;
    std::size_t InUse() const noexcept;

private:
    struct State;
    std::shared_ptr<State> state_;
};

} // namespace visionai::edge::memory
