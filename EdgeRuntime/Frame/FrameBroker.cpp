#include "Frame/FrameBroker.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace visionai::edge::frame {

FrameBroker::FrameBroker(std::size_t max_subscribers)
    : snapshot_(std::make_shared<const Snapshot>()),
      max_subscribers_(std::max<std::size_t>(1, max_subscribers)) {}

FrameBroker::SubscriberId FrameBroker::Subscribe(Callback callback) {
    if (!callback) {
        // 0 被保留为“无效订阅编号”，便于调用方判断注册失败。
        return 0;
    }

    std::lock_guard lock(update_mutex_);
    if (next_id_ == std::numeric_limits<SubscriberId>::max()) {
        // 理论上几乎不可能达到，但宁可拒绝新订阅，也不能回绕后覆盖仍在使用的订阅编号。
        return 0;
    }

    const auto current = snapshot_.load(std::memory_order_acquire);
    if (current->size() >= max_subscribers_) {
        // 防止错误的重复订阅把回调闭包及其捕获资源无限保留在 Broker（分发器）生命周期内。
        return 0;
    }

    const auto id = next_id_++;
    auto next = std::make_shared<Snapshot>(*current);
    next->push_back(Subscriber{id, std::move(callback)});

    // 发布不可变快照；正在 Publish 的线程仍可安全使用旧快照。
    snapshot_.store(std::const_pointer_cast<const Snapshot>(next), std::memory_order_release);
    return id;
}

void FrameBroker::Unsubscribe(SubscriberId id) {
    if (id == 0) {
        return;
    }

    std::lock_guard lock(update_mutex_);
    const auto current = snapshot_.load(std::memory_order_acquire);

    const auto it = std::find_if(
        current->begin(),
        current->end(),
        [id](const Subscriber& subscriber) { return subscriber.id == id; });
    if (it == current->end()) {
        return;
    }

    auto next = std::make_shared<Snapshot>();
    next->reserve(current->size() - 1);
    for (const auto& subscriber : *current) {
        if (subscriber.id != id) {
            next->push_back(subscriber);
        }
    }
    snapshot_.store(std::const_pointer_cast<const Snapshot>(next), std::memory_order_release);
}

void FrameBroker::Clear() {
    std::lock_guard lock(update_mutex_);
    snapshot_.store(std::make_shared<const Snapshot>(), std::memory_order_release);
}

void FrameBroker::Publish(const contracts::media::FrameRef& frame) noexcept {
    const auto current = snapshot_.load(std::memory_order_acquire);
    for (const auto& subscriber : *current) {
        try {
            subscriber.callback(frame);
        } catch (...) {
            // 数据面不能因为单个插件/算法消费者异常而中断整路视频。
            // 这里不引入日志依赖，只累计指标，由上层 Observability（可观测体系）统一采集。
            failed_callbacks_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

std::size_t FrameBroker::SubscriberCount() const noexcept {
    return snapshot_.load(std::memory_order_acquire)->size();
}

std::uint64_t FrameBroker::FailedCallbackCount() const noexcept {
    return failed_callbacks_.load(std::memory_order_relaxed);
}

} // namespace visionai::edge::frame
