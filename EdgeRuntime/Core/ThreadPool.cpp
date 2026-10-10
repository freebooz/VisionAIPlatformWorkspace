#include "Core/ThreadPool.hpp"

#include <stdexcept>
#include <utility>

namespace visionai::edge::core {

ThreadPool::ThreadPool(
    std::size_t worker_count,
    std::size_t queue_capacity)
    : queue_(queue_capacity) {
    if (worker_count == 0) {
        throw std::invalid_argument("ThreadPool worker_count 必须大于 0");
    }

    if (!lifecycle_.TryStart()) {
        throw std::logic_error("ThreadPool 生命周期初始化失败");
    }

    try {
        workers_.reserve(worker_count);

        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back(
                [this, i] {
                    WorkerLoop(i);
                });
        }

        lifecycle_.MarkRunning();
    } catch (...) {
        // 如果线程创建中途失败，必须关闭队列并回收已经创建的线程，避免构造失败留下后台线程。
        lifecycle_.MarkFailed();
        queue_.CloseAndDiscard();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        throw;
    }
}

ThreadPool::~ThreadPool() noexcept {
    Stop(true);
}

TaskSubmitResult ThreadPool::TrySubmit(Task task) {
    if (!task) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return TaskSubmitResult::Stopped;
    }

    if (!lifecycle_.AcceptingWork()) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return TaskSubmitResult::Stopped;
    }

    const auto result = ConvertQueueResult(queue_.TryPush(std::move(task)));

    if (result == TaskSubmitResult::Accepted) {
        submitted_.fetch_add(1, std::memory_order_relaxed);
    } else {
        rejected_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

TaskSubmitResult ThreadPool::SubmitFor(
    Task task,
    std::chrono::milliseconds timeout) {
    if (!task) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return TaskSubmitResult::Stopped;
    }

    if (!lifecycle_.AcceptingWork()) {
        rejected_.fetch_add(1, std::memory_order_relaxed);
        return TaskSubmitResult::Stopped;
    }

    const auto result =
        ConvertQueueResult(queue_.PushFor(std::move(task), timeout));

    if (result == TaskSubmitResult::Accepted) {
        submitted_.fetch_add(1, std::memory_order_relaxed);
    } else {
        rejected_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

void ThreadPool::Stop(bool drain) noexcept {
    // 多个控制线程同时请求停止时串行化 join（线程等待）过程。
    std::lock_guard stop_lock(stop_mutex_);

    const auto state = lifecycle_.State();
    if (state == LifecycleState::Stopped) {
        return;
    }

    lifecycle_.RequestStop();

    if (drain) {
        queue_.Close();
    } else {
        // 紧急停止时丢弃的任务也计入 rejected（拒绝/丢弃），方便监控数据损失。
        const auto discarded = queue_.CloseAndDiscard();
        rejected_.fetch_add(
            static_cast<std::uint64_t>(discarded),
            std::memory_order_relaxed);
    }

    const auto caller = std::this_thread::get_id();

    // 若任务错误地在 worker（工作线程）内部调用 Stop，不能 join 自身。
    // 此时标记 Failed（失败）并返回，工作线程将在当前任务结束后因队列关闭而自然退出；
    // 最终 join 必须由持有 ThreadPool 所有权的外部控制线程完成。
    for (const auto& worker : workers_) {
        if (worker.joinable() && worker.get_id() == caller) {
            lifecycle_.MarkFailed();
            return;
        }
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    lifecycle_.MarkStopped();
}

ThreadPoolMetrics ThreadPool::Metrics() const {
    return ThreadPoolMetrics{
        .submitted = submitted_.load(std::memory_order_relaxed),
        .completed = completed_.load(std::memory_order_relaxed),
        .failed = failed_.load(std::memory_order_relaxed),
        .rejected = rejected_.load(std::memory_order_relaxed),
        .active = active_.load(std::memory_order_relaxed),
        .queued = queue_.Size(),
        .queue_capacity = queue_.Capacity(),
        .lifecycle = lifecycle_.State()
    };
}

void ThreadPool::WorkerLoop(std::size_t worker_index) noexcept {
    // worker_index 预留给后续线程命名和 per-worker（每线程）指标。
    (void)worker_index;

    while (true) {
        auto task = queue_.WaitPop();

        if (!task.has_value()) {
            return;
        }

        active_.fetch_add(1, std::memory_order_relaxed);

        try {
            (*task)();
            completed_.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
            // 插件/任务异常必须隔离在执行器边界内。
            failed_.fetch_add(1, std::memory_order_relaxed);
        }

        active_.fetch_sub(1, std::memory_order_relaxed);
    }
}

TaskSubmitResult ThreadPool::ConvertQueueResult(
    QueuePushResult result) noexcept {
    switch (result) {
        case QueuePushResult::Enqueued:
            return TaskSubmitResult::Accepted;
        case QueuePushResult::Full:
            return TaskSubmitResult::QueueFull;
        case QueuePushResult::Timeout:
            return TaskSubmitResult::Timeout;
        case QueuePushResult::Closed:
            return TaskSubmitResult::Stopped;
    }

    return TaskSubmitResult::Stopped;
}

} // namespace visionai::edge::core
