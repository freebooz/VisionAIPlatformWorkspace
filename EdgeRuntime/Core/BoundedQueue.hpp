#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace visionai::edge::core {

// QueuePushResult（入队结果）用于区分成功、容量不足、超时和队列关闭。
// 调用方应根据结果执行降级、丢帧、回压或告警，禁止静默无限积压。
enum class QueuePushResult {
    Enqueued = 0,
    Full,
    Timeout,
    Closed
};

// BoundedQueue（有界阻塞队列）是边缘运行时统一的任务/消息排队基础设施。
//
// 生产约束：
// 1. 队列容量在构造时固定，禁止无界增长。
// 2. Close（关闭）后不再接受新元素，但默认允许消费者继续排空已存在元素。
// 3. CloseAndDiscard（关闭并丢弃）用于紧急停止，不应作为正常优雅停机路径。
// 4. 所有公开方法线程安全；回调/业务逻辑不得在队列内部锁持有期间执行。
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity)
        : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue 容量必须大于 0");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // TryPush（立即入队）不等待空间，适用于视频帧等实时数据路径。
    QueuePushResult TryPush(T value) {
        {
            std::lock_guard lock(mutex_);

            if (closed_) {
                return QueuePushResult::Closed;
            }
            if (queue_.size() >= capacity_) {
                return QueuePushResult::Full;
            }

            queue_.push_back(std::move(value));
        }

        // 解锁后唤醒消费者，减少无效线程切换。
        not_empty_.notify_one();
        return QueuePushResult::Enqueued;
    }

    // PushFor（限时等待入队）用于必须尽量保留的控制任务。
    // timeout <= 0 时退化为 TryPush，避免意外永久阻塞生产线程。
    template <typename Rep, typename Period>
    QueuePushResult PushFor(
        T value,
        std::chrono::duration<Rep, Period> timeout) {
        if (timeout <= timeout.zero()) {
            return TryPush(std::move(value));
        }

        std::unique_lock lock(mutex_);
        const bool ready = not_full_.wait_for(
            lock,
            timeout,
            [this] {
                return closed_ || queue_.size() < capacity_;
            });

        if (!ready) {
            return QueuePushResult::Timeout;
        }
        if (closed_) {
            return QueuePushResult::Closed;
        }

        queue_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
        return QueuePushResult::Enqueued;
    }

    // WaitPop（等待出队）在队列关闭且已排空时返回 std::nullopt，
    // 使工作线程能够自然退出而不需要轮询。
    std::optional<T> WaitPop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(
            lock,
            [this] {
                return closed_ || !queue_.empty();
            });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    // TryPop（立即出队）适合非阻塞诊断和测试路径。
    std::optional<T> TryPop() {
        std::optional<T> value;

        {
            std::lock_guard lock(mutex_);

            if (queue_.empty()) {
                return std::nullopt;
            }

            value.emplace(std::move(queue_.front()));
            queue_.pop_front();
        }

        not_full_.notify_one();
        return value;
    }

    // Close（关闭）停止接收新元素，并唤醒全部生产者/消费者。
    // 已排队元素仍可被消费者继续处理，实现 drain（排空式优雅停机）。
    void Close() noexcept {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }

        not_empty_.notify_all();
        not_full_.notify_all();
    }

    // CloseAndDiscard（关闭并丢弃）用于故障紧急停止。
    // 返回被丢弃元素数量，便于 Observability（可观测体系）记录数据损失。
    std::size_t CloseAndDiscard() noexcept {
        std::size_t discarded = 0;

        {
            std::lock_guard lock(mutex_);
            closed_ = true;
            discarded = queue_.size();
            // 紧急停止优先保证 noexcept（不抛异常）语义。
            // clear 在锁内执行可能稍慢，但该路径不是正常实时热路径，
            // 且避免为了“锁外析构”额外构造临时 deque 时发生内存分配失败。
            queue_.clear();
        }

        not_empty_.notify_all();
        not_full_.notify_all();
        return discarded;
    }

    std::size_t Size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    std::size_t Capacity() const noexcept {
        return capacity_;
    }

    bool Closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

private:
    const std::size_t capacity_;

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    bool closed_{false};
};

} // namespace visionai::edge::core
