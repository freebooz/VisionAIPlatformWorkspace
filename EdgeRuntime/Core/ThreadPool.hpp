#pragma once

#include "Core/BoundedQueue.hpp"
#include "Core/Lifecycle.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace visionai::edge::core {

// TaskSubmitResult（任务提交结果）要求上层明确处理回压，而不是让任务无限进入内存。
enum class TaskSubmitResult {
    Accepted = 0,
    QueueFull,
    Timeout,
    Stopped
};

// ThreadPoolMetrics（线程池指标快照）用于 Prometheus/OpenTelemetry（监控/开放遥测）采集。
struct ThreadPoolMetrics {
    std::uint64_t submitted{};
    std::uint64_t completed{};
    std::uint64_t failed{};
    std::uint64_t rejected{};
    std::uint64_t active{};
    std::size_t queued{};
    std::size_t queue_capacity{};
    LifecycleState lifecycle{LifecycleState::Created};
};

// ThreadPool（有界线程池）用于边缘运行时 CPU 任务执行。
//
// 生产原则：
// - 队列固定容量，避免请求峰值导致内存无限增长。
// - 工作线程数量固定，避免高负载时无限创建线程。
// - 任务异常在工作线程边界内捕获，不允许单个插件异常导致 std::terminate（进程终止）。
// - 析构函数执行 drain=true 的优雅停止，保证不会泄漏线程。
// - Stop(false) 仅用于紧急故障路径，会丢弃尚未执行任务。
// - ThreadPool 的所有权必须由宿主控制线程持有，禁止任务回调销毁自身线程池。
class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(std::size_t worker_count, std::size_t queue_capacity);
    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    TaskSubmitResult TrySubmit(Task task);

    TaskSubmitResult SubmitFor(
        Task task,
        std::chrono::milliseconds timeout);

    // Stop（停止）可重复调用。
    // drain=true：不接收新任务，等待队列剩余任务全部执行完；
    // drain=false：清空待执行任务后停止。
    void Stop(bool drain = true) noexcept;

    ThreadPoolMetrics Metrics() const;

    std::size_t WorkerCount() const noexcept {
        return workers_.size();
    }

private:
    void WorkerLoop(std::size_t worker_index) noexcept;
    TaskSubmitResult ConvertQueueResult(QueuePushResult result) noexcept;

    BoundedQueue<Task> queue_;
    LifecycleController lifecycle_;
    std::vector<std::thread> workers_;

    // stop_mutex_（停止互斥锁）确保并发 Stop 请求不会同时 join 同一工作线程。
    mutable std::mutex stop_mutex_;

    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> rejected_{0};
    std::atomic<std::uint64_t> active_{0};
};

} // namespace visionai::edge::core
