#include "Core/BoundedQueue.hpp"
#include "Core/Lifecycle.hpp"
#include "Core/ThreadPool.hpp"
#include "Observability/HealthRegistry.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

// CHECK（始终生效的测试断言）不依赖 NDEBUG，Release（发布）构建同样执行。
[[noreturn]] void FailCheck(
    const char* expression,
    const char* file,
    int line) {
    throw std::runtime_error(
        std::string("CHECK failed: ") +
        expression +
        " @ " +
        file +
        ":" +
        std::to_string(line));
}

#define CHECK(expression) \
    ((expression) ? static_cast<void>(0) : FailCheck(#expression, __FILE__, __LINE__))

// TestBoundedQueue（有界队列测试）验证容量、关闭、排空和紧急丢弃语义。
void TestBoundedQueue() {
    using namespace visionai::edge::core;

    BoundedQueue<int> queue(2);

    CHECK(queue.Capacity() == 2);
    CHECK(queue.TryPush(1) == QueuePushResult::Enqueued);
    CHECK(queue.TryPush(2) == QueuePushResult::Enqueued);
    CHECK(queue.TryPush(3) == QueuePushResult::Full);
    CHECK(queue.Size() == 2);

    // 队列保持满载且没有消费者释放空间时，限时入队必须明确返回 Timeout（超时）。
    CHECK(
        queue.PushFor(
            3,
            std::chrono::milliseconds(2)) ==
        QueuePushResult::Timeout);

    const auto first = queue.TryPop();
    CHECK(first.has_value());
    CHECK(*first == 1);

    CHECK(
        queue.PushFor(
            3,
            std::chrono::milliseconds(10)) ==
        QueuePushResult::Enqueued);

    queue.Close();
    CHECK(queue.Closed());
    CHECK(queue.TryPush(4) == QueuePushResult::Closed);

    const auto second = queue.WaitPop();
    const auto third = queue.WaitPop();
    const auto finished = queue.WaitPop();

    CHECK(second.has_value() && *second == 2);
    CHECK(third.has_value() && *third == 3);
    CHECK(!finished.has_value());

    BoundedQueue<int> discard_queue(4);
    CHECK(discard_queue.TryPush(10) == QueuePushResult::Enqueued);
    CHECK(discard_queue.TryPush(20) == QueuePushResult::Enqueued);
    CHECK(discard_queue.CloseAndDiscard() == 2);
    CHECK(discard_queue.Size() == 0);
}

// TestLifecycle（生命周期测试）验证组件只能按受控方向进入运行和停止状态。
void TestLifecycle() {
    using namespace visionai::edge::core;

    LifecycleController lifecycle;
    CHECK(lifecycle.State() == LifecycleState::Created);
    CHECK(lifecycle.TryStart());
    CHECK(!lifecycle.TryStart());

    lifecycle.MarkRunning();
    CHECK(lifecycle.AcceptingWork());

    CHECK(lifecycle.RequestStop());
    CHECK(lifecycle.StopRequested());
    CHECK(!lifecycle.AcceptingWork());

    lifecycle.MarkStopped();
    CHECK(lifecycle.State() == LifecycleState::Stopped);
}

// TestThreadPoolDrain（线程池优雅排空测试）验证任务全部执行、异常隔离和线程回收。
void TestThreadPoolDrain() {
    using namespace visionai::edge::core;

    ThreadPool pool(4, 64);
    std::atomic<int> completed_work{0};

    for (int i = 0; i < 1000; ++i) {
        const auto result = pool.SubmitFor(
            [&completed_work] {
                completed_work.fetch_add(1, std::memory_order_relaxed);
            },
            std::chrono::milliseconds(100));

        CHECK(result == TaskSubmitResult::Accepted);
    }

    const auto throwing_result = pool.SubmitFor(
        [] {
            throw std::runtime_error("预期的任务异常");
        },
        std::chrono::milliseconds(100));
    CHECK(throwing_result == TaskSubmitResult::Accepted);

    pool.Stop(true);

    const auto metrics = pool.Metrics();
    CHECK(completed_work.load(std::memory_order_relaxed) == 1000);
    CHECK(metrics.submitted == 1001);
    CHECK(metrics.completed == 1000);
    CHECK(metrics.failed == 1);
    CHECK(metrics.active == 0);
    CHECK(metrics.queued == 0);
    CHECK(metrics.lifecycle == LifecycleState::Stopped);

    // 停止后新任务必须被明确拒绝。
    CHECK(
        pool.TrySubmit([] {}) ==
        TaskSubmitResult::Stopped);
}

// TestThreadPoolBackpressure（线程池回压测试）验证队列满时不会继续无限积压。
void TestThreadPoolBackpressure() {
    using namespace visionai::edge::core;

    ThreadPool pool(1, 1);

    std::promise<void> worker_started;
    std::promise<void> release_worker;
    auto release_future = release_worker.get_future().share();

    CHECK(
        pool.TrySubmit(
            [&worker_started, release_future] {
                worker_started.set_value();
                release_future.wait();
            }) ==
        TaskSubmitResult::Accepted);

    // 确认唯一工作线程已被第一项任务占用。
    worker_started.get_future().wait();

    CHECK(
        pool.TrySubmit([] {}) ==
        TaskSubmitResult::Accepted);

    // 一条执行、一条排队后，第三条必须收到 QueueFull（队列已满），不能继续申请内存积压。
    CHECK(
        pool.TrySubmit([] {}) ==
        TaskSubmitResult::QueueFull);

    release_worker.set_value();
    pool.Stop(true);

    const auto metrics = pool.Metrics();
    CHECK(metrics.rejected >= 1);
    CHECK(metrics.completed == 2);
}

// TestThreadPoolConcurrentSubmit（并发提交测试）验证多个生产线程同时提交任务时计数和生命周期稳定。
void TestThreadPoolConcurrentSubmit() {
    using namespace visionai::edge::core;

    ThreadPool pool(4, 128);
    std::atomic<std::uint64_t> executed{0};

    constexpr int kProducerCount = 8;
    constexpr int kTasksPerProducer = 250;

    std::vector<std::thread> producers;
    producers.reserve(kProducerCount);

    for (int producer = 0; producer < kProducerCount; ++producer) {
        producers.emplace_back([&pool, &executed] {
            for (int i = 0; i < kTasksPerProducer; ++i) {
                while (true) {
                    const auto result = pool.SubmitFor(
                        [&executed] {
                            executed.fetch_add(1, std::memory_order_relaxed);
                        },
                        std::chrono::milliseconds(50));

                    if (result == TaskSubmitResult::Accepted) {
                        break;
                    }

                    CHECK(
                        result == TaskSubmitResult::QueueFull ||
                        result == TaskSubmitResult::Timeout);
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    pool.Stop(true);

    CHECK(
        executed.load(std::memory_order_relaxed) ==
        static_cast<std::uint64_t>(
            kProducerCount * kTasksPerProducer));

    const auto metrics = pool.Metrics();
    CHECK(metrics.active == 0);
    CHECK(metrics.queued == 0);
    CHECK(metrics.lifecycle == LifecycleState::Stopped);
}

// TestThreadPoolLifecycleStress（线程池生命周期压力测试）反复创建/销毁线程池，
// 用于尽早发现线程未回收、关闭竞态和任务排空遗漏。
void TestThreadPoolLifecycleStress() {
    using namespace visionai::edge::core;

    constexpr int kCycles = 100;
    constexpr int kTasksPerCycle = 32;

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        std::atomic<int> executed{0};

        {
            ThreadPool pool(2, 16);

            for (int i = 0; i < kTasksPerCycle; ++i) {
                const auto result = pool.SubmitFor(
                    [&executed] {
                        executed.fetch_add(1, std::memory_order_relaxed);
                    },
                    std::chrono::milliseconds(50));

                CHECK(result == TaskSubmitResult::Accepted);
            }

            // 不显式调用 Stop，验证析构函数能够完成 drain（排空）和 join（线程回收）。
        }

        CHECK(executed.load(std::memory_order_relaxed) == kTasksPerCycle);
    }
}

// TestHealthRegistry（健康注册表测试）验证容量边界、总体状态和确定性快照顺序。
void TestHealthRegistry() {
    using namespace visionai::edge::observability;

    HealthRegistry registry(2);

    CHECK(
        registry.Update(
            "decoder",
            HealthStatus::Healthy,
            "解码器运行正常"));

    CHECK(
        registry.Update(
            "runtime",
            HealthStatus::Degraded,
            "GPU（图形处理器）负载偏高"));

    // 容量达到上限后拒绝新的未知组件，但允许更新已存在组件。
    CHECK(
        !registry.Update(
            "extra",
            HealthStatus::Healthy));

    CHECK(
        registry.Update(
            "decoder",
            HealthStatus::Unhealthy,
            "视频流连续解码失败"));

    CHECK(registry.Size() == 2);
    CHECK(registry.OverallStatus() == HealthStatus::Unhealthy);

    const auto snapshot = registry.Snapshot();
    CHECK(snapshot.size() == 2);
    CHECK(snapshot[0].component == "decoder");
    CHECK(snapshot[1].component == "runtime");

    CHECK(registry.Remove("decoder"));
    CHECK(registry.Size() == 1);
    CHECK(registry.OverallStatus() == HealthStatus::Degraded);
}

} // namespace

int main() {
    try {
        std::cerr << "[TEST] BoundedQueue（有界队列）" << std::endl;
        TestBoundedQueue();

        std::cerr << "[TEST] Lifecycle（生命周期）" << std::endl;
        TestLifecycle();

        std::cerr << "[TEST] ThreadPool Drain（线程池排空）" << std::endl;
        TestThreadPoolDrain();

        std::cerr << "[TEST] ThreadPool Backpressure（线程池回压）" << std::endl;
        TestThreadPoolBackpressure();

        std::cerr << "[TEST] ThreadPool Concurrency（线程池并发）" << std::endl;
        TestThreadPoolConcurrentSubmit();

        std::cerr << "[TEST] ThreadPool Lifecycle Stress（线程池生命周期压力）" << std::endl;
        TestThreadPoolLifecycleStress();

        std::cerr << "[TEST] HealthRegistry（健康注册表）" << std::endl;
        TestHealthRegistry();

        std::cout << "VisionAI runtime primitives tests PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "[FAIL] 未知异常" << std::endl;
        return 3;
    }
}
