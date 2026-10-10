#pragma once

#include "Media/FrameRef.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace visionai::edge::frame {

// FrameBroker（视频帧分发器）把同一个 FrameRef（视频帧引用）广播给多个消费者。
//
// 性能策略：
// - Subscribe/Unsubscribe（订阅/取消订阅）属于低频控制面操作，会重建一次不可变快照；
// - Publish（发布帧）属于高频数据面操作，只做一次 atomic shared_ptr（原子共享指针）读取，
//   不获取互斥锁，也不逐帧复制 std::function，从而避免高帧率下的锁竞争和堆分配。
//
// 稳定性策略：
// - 空回调不会注册；
// - 单个订阅者抛出异常不会中断其他订阅者；
// - FailedCallbackCount（失败回调计数）供监控侧发现异常消费者。
class FrameBroker {
public:
    using SubscriberId = std::uint64_t;
    using Callback = std::function<void(const contracts::media::FrameRef&)>;

    explicit FrameBroker(std::size_t max_subscribers = 1024);

    SubscriberId Subscribe(Callback callback);
    void Unsubscribe(SubscriberId id);
    void Clear();

    void Publish(const contracts::media::FrameRef& frame) noexcept;
    std::size_t SubscriberCount() const noexcept;
    std::uint64_t FailedCallbackCount() const noexcept;

private:
    struct Subscriber {
        SubscriberId id{};
        Callback callback;
    };

    using Snapshot = std::vector<Subscriber>;

    // update_mutex_ 只保护订阅快照更新，不参与每帧 Publish。
    mutable std::mutex update_mutex_;
    std::atomic<std::shared_ptr<const Snapshot>> snapshot_;
    SubscriberId next_id_{1};
    std::size_t max_subscribers_;
    std::atomic<std::uint64_t> failed_callbacks_{0};
};

} // namespace visionai::edge::frame
