#include "Memory/BufferPool.hpp"

#include <limits>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace visionai::edge::memory {

struct BufferPool::State {
    std::size_t block_size{};
    std::vector<std::unique_ptr<std::byte[]>> blocks;
    std::vector<std::size_t> free_indices;
    mutable std::mutex mutex;
};

BufferPool::BufferPool(std::size_t block_size, std::size_t block_count)
    : state_(std::make_shared<State>()) {
    // 参数为 0 会形成无法工作的池，直接失败比运行期静默返回空缓冲更容易定位配置错误。
    if (block_size == 0 || block_count == 0) {
        throw std::invalid_argument("BufferPool 参数必须大于 0");
    }

    // 防止 block_size * block_count 的理论容量在 size_t 上溢出。
    if (block_count > (std::numeric_limits<std::size_t>::max() / block_size)) {
        throw std::overflow_error("BufferPool 总容量计算溢出");
    }

    state_->block_size = block_size;
    state_->blocks.reserve(block_count);
    state_->free_indices.reserve(block_count);

    // 大块像素内存在构造阶段一次性完成分配，避免视频运行过程中频繁 malloc/free。
    for (std::size_t i = 0; i < block_count; ++i) {
        state_->blocks.push_back(std::make_unique<std::byte[]>(block_size));
        state_->free_indices.push_back(block_count - 1 - i);
    }
}

SharedBuffer BufferPool::Acquire() {
    std::size_t index{};
    std::byte* data{};

    // 锁只覆盖空闲索引的弹出，不覆盖后续租约控制块构造，缩短高并发临界区。
    {
        std::lock_guard lock(state_->mutex);
        if (state_->free_indices.empty()) {
            return {};
        }
        index = state_->free_indices.back();
        state_->free_indices.pop_back();
        data = state_->blocks[index].get();
    }

    // shared_ptr（共享指针）控制块仍会产生一次小对象分配，但不会复制视频像素。
    // 这里优先选择标准 shared_ptr 生命周期模型，避免自定义全局 PMR（多态内存资源）
    // 在进程静态析构阶段产生 allocator（分配器）生命周期顺序风险。
    //
    // 若控制块分配失败，标准 shared_ptr 构造保证调用 deleter（删除器），索引仍会被归还。
    auto state = state_;
    std::shared_ptr<void> owner(
        data,
        [state, index](void*) noexcept {
            // free_indices 在构造阶段已经 reserve 到 Capacity（容量），归还时不会再次扩容。
            std::lock_guard lock(state->mutex);
            state->free_indices.push_back(index);
        });

    return {std::move(owner), data, state_->block_size};
}

std::size_t BufferPool::BlockSize() const noexcept { return state_->block_size; }
std::size_t BufferPool::Capacity() const noexcept { return state_->blocks.size(); }

std::size_t BufferPool::Available() const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->free_indices.size();
}

std::size_t BufferPool::InUse() const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->blocks.size() - state_->free_indices.size();
}

} // namespace visionai::edge::memory
