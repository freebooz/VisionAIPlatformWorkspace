#include "Capability/ObjectSet.hpp"
#include "EventEngineLite/EventEngine.hpp"
#include "Frame/FrameBroker.hpp"
#include "Graph/GraphExecutor.hpp"
#include "Memory/BufferPool.hpp"
#include "RuleEngineLite/RuleEngine.hpp"
#include "Scheduler/ExecutionScheduler.hpp"
#include "Spatial/ZoneMembership.hpp"
#include "Temporal/DurationTracker.hpp"
#include "Tracking/SimpleIoUTracker.hpp"

#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {


// CHECK（始终生效的测试断言）不受 NDEBUG / Release（发布构建）影响。
// 标准 assert 在 Release 中会被编译器移除，不能用于稳定性、ASAN（地址消毒器）或发布构建验证。
[[noreturn]] void FailCheck(const char* expression, const char* file, int line) {
    throw std::runtime_error(
        std::string("CHECK failed: ") + expression + " @ " + file + ":" + std::to_string(line));
}

#define CHECK(expression) \
    ((expression) ? static_cast<void>(0) : FailCheck(#expression, __FILE__, __LINE__))

// TestBufferPool（缓冲池测试）验证容量归还、共享所有权和并发租约不会导致块永久丢失。
void TestBufferPool() {
    using namespace visionai;

    edge::memory::BufferPool pool(1024, 16);
    CHECK(pool.Capacity() == 16);
    CHECK(pool.Available() == 16);
    CHECK(pool.InUse() == 0);

    {
        auto first = pool.Acquire();
        auto second = first; // 复制租约只增加引用计数，不应提前归还底层块。
        CHECK(first && second);
        CHECK(pool.InUse() == 1);

        first.owner.reset();
        CHECK(pool.InUse() == 1);

        second.owner.reset();
        CHECK(pool.InUse() == 0);
        CHECK(pool.Available() == pool.Capacity());
    }

    // 多线程压力测试：反复申请/释放同一小型池，结束后必须全部归还。
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back([&] {
            for (int i = 0; i < 2000; ++i) {
                edge::memory::SharedBuffer buffer;
                for (int retry = 0; retry < 100 && !buffer; ++retry) {
                    buffer = pool.Acquire();
                    if (!buffer) {
                        std::this_thread::yield();
                    }
                }

                if (!buffer) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }

                buffer.data[0] = std::byte{0x2A};
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    CHECK(!failed.load(std::memory_order_relaxed));
    CHECK(pool.InUse() == 0);
    CHECK(pool.Available() == pool.Capacity());
}

// TestFrameBroker（视频帧分发器测试）验证零拷贝共享、异常隔离和订阅清理。
void TestFrameBroker() {
    using namespace visionai;

    edge::memory::BufferPool pool(1024, 1);
    auto buffer = pool.Acquire();
    CHECK(buffer);

    contracts::media::FrameRef frame;
    frame.stream_id = 1;
    frame.frame_id = 7;
    frame.width = 16;
    frame.height = 16;
    frame.data = buffer.data;
    frame.bytes = buffer.bytes;
    frame.owner = buffer.owner;

    edge::frame::FrameBroker broker;

    void* first_address = nullptr;
    void* second_address = nullptr;

    const auto first_id = broker.Subscribe(
        [&](const auto& item) { first_address = item.data; });
    CHECK(first_id != 0);

    // 故意抛异常，验证坏插件不会阻断后续消费者。
    const auto failing_id = broker.Subscribe(
        [](const auto&) { throw std::runtime_error("test callback failure"); });
    CHECK(failing_id != 0);

    const auto second_id = broker.Subscribe(
        [&](const auto& item) { second_address = item.data; });
    CHECK(second_id != 0);

    broker.Publish(frame);

    CHECK(first_address == frame.data);
    CHECK(second_address == frame.data);
    CHECK(first_address == second_address);
    CHECK(broker.FailedCallbackCount() == 1);
    CHECK(broker.SubscriberCount() == 3);

    broker.Unsubscribe(failing_id);
    CHECK(broker.SubscriberCount() == 2);

    broker.Clear();
    CHECK(broker.SubscriberCount() == 0);
}

// TestGraphExecutor（DAG 执行器测试）验证拓扑顺序、冻结语义和异常边界。
void TestGraphExecutor() {
    using namespace visionai;

    edge::graph::GraphExecutor graph;
    int sequence = 0;

    CHECK(graph.AddNode("detect", {}, [&] {
        sequence = 1;
        return true;
    }));

    CHECK(graph.AddNode("track", {"detect"}, [&] {
        CHECK(sequence == 1);
        sequence = 2;
        return true;
    }));

    CHECK(graph.AddNode("rule", {"track"}, [&] {
        CHECK(sequence == 2);
        sequence = 3;
        return true;
    }));

    std::string error;
    CHECK(graph.Validate(&error));
    CHECK(graph.Execute(&error));
    CHECK(sequence == 3);

    // 首次执行后图被冻结，运行期新增节点必须被拒绝。
    CHECK(!graph.AddNode("late-node", {}, [] { return true; }));

    edge::graph::GraphExecutor duplicate_dependency;
    CHECK(duplicate_dependency.AddNode("source", {}, [] { return true; }));
    CHECK(!duplicate_dependency.AddNode(
        "bad",
        {"source", "source"},
        [] { return true; }));

    edge::graph::GraphExecutor throwing_graph;
    CHECK(throwing_graph.AddNode("thrower", {}, []() -> bool {
        throw std::runtime_error("expected");
    }));

    error.clear();
    CHECK(!throwing_graph.Execute(&error));
    CHECK(error.find("抛出异常") != std::string::npos);
}

// TestScheduler（执行调度器测试）验证 FPS 限流、异常配置、时间回退和状态容量上限。
void TestScheduler() {
    using namespace visionai;

    edge::scheduler::ExecutionScheduler scheduler(2);

    CHECK(scheduler.ShouldRun("detector", 0, 5.0));
    CHECK(!scheduler.ShouldRun("detector", 100'000'000, 5.0));
    CHECK(scheduler.ShouldRun("detector", 200'000'000, 5.0));

    // 时间戳回退后应立即重置调度基线，而不是长时间跳过。
    CHECK(scheduler.ShouldRun("detector", 10, 5.0));

    // 非法 FPS 直接拒绝。
    CHECK(!scheduler.ShouldRun(
        "bad",
        0,
        std::numeric_limits<double>::quiet_NaN()));

    CHECK(scheduler.ShouldRun("a", 1, 1.0));
    CHECK(scheduler.ShouldRun("b", 2, 1.0));
    CHECK(scheduler.StateCount() <= 2);

    scheduler.ResetAll();
    CHECK(scheduler.StateCount() == 0);
}

// BuildPersonDetections（构造人员检测结果）集中生成跟踪测试输入，减少测试代码重复。
visionai::contracts::capability::ObjectSet BuildPersonDetections() {
    visionai::contracts::capability::ObjectSet objects;
    objects.stream_id = 1;
    objects.frame_id = 1;
    objects.timestamp_ns = 0;
    objects.objects.push_back({
        1,
        0,
        "person",
        0.95F,
        {10, 10, 50, 100}
    });
    return objects;
}

// TestTrackingAndSpatial（跟踪与空间测试）验证轨迹保持、坏框过滤和区域判断。
void TestTrackingAndSpatial() {
    using namespace visionai;

    auto objects = BuildPersonDetections();

    algorithms::tracking::SimpleIoUTracker tracker;
    auto tracks = tracker.Update(objects);
    CHECK(tracks.tracks.size() == 1);

    const auto track_id = tracks.tracks.front().track_id;

    objects.frame_id = 2;
    objects.timestamp_ns = 40'000'000;
    objects.objects.front().bbox = {12, 10, 52, 100};

    tracks = tracker.Update(objects);
    CHECK(tracks.tracks.size() == 1);
    CHECK(tracks.tracks.front().track_id == track_id);

    // 注入 NaN 目标框，跟踪器应稳定过滤，不产生异常轨迹。
    objects.objects.push_back({
        2,
        0,
        "person",
        0.9F,
        {std::numeric_limits<float>::quiet_NaN(), 0, 20, 30}
    });

    tracks = tracker.Update(objects);
    CHECK(tracks.tracks.size() == 1);
    CHECK(tracker.ActiveTrackCount() == 1);

    algorithms::spatial::ZoneMembership zone({
        {0, 0},
        {100, 0},
        {100, 120},
        {0, 120}
    });

    const auto zone_states = zone.Evaluate(tracks);
    CHECK(zone_states.size() == 1);
    CHECK(zone_states.front().inside);

    // 无效多边形不抛异常，统一返回区域外。
    algorithms::spatial::ZoneMembership invalid_zone({
        {0, 0},
        {10, 0}
    });
    const auto invalid_states = invalid_zone.Evaluate(tracks);
    CHECK(!invalid_states.front().inside);

    tracker.Reset();
    CHECK(tracker.ActiveTrackCount() == 0);
}

// TestDurationRuleAndEvent（持续时间、规则和事件测试）覆盖时间回退、状态上限、NaN 和并发事件发布。
void TestDurationRuleAndEvent() {
    using namespace visionai;

    algorithms::temporal::DurationTracker duration(2);
    CHECK(duration.Update(1, true, 0) == 0.0);
    CHECK(duration.Update(1, true, 2'000'000'000) >= 2.0);

    // 时间戳回退时重新开始计时。
    CHECK(duration.Update(1, true, 100) == 0.0);

    CHECK(duration.Update(2, true, 200) == 0.0);
    CHECK(duration.Update(3, true, 300) == 0.0);
    CHECK(duration.TrackedKeyCount() <= 2);

    duration.ResetAll();
    CHECK(duration.TrackedKeyCount() == 0);

    edge::rule::RuleEngine rules;
    const std::unordered_map<std::string, double> facts{
        {"subject_present", 1},
        {"guard_count", 0},
        {"guard_absent_seconds", 6}
    };
    const std::vector<edge::rule::Condition> conditions{
        {"subject_present", edge::rule::ComparisonOp::Ge, 1},
        {"guard_count", edge::rule::ComparisonOp::Lt, 1},
        {"guard_absent_seconds", edge::rule::ComparisonOp::Ge, 5}
    };
    CHECK(rules.EvaluateAll(conditions, facts));

    const std::unordered_map<std::string, double> bad_facts{
        {"subject_present", std::numeric_limits<double>::quiet_NaN()}
    };
    const std::vector<edge::rule::Condition> bad_conditions{
        {"subject_present", edge::rule::ComparisonOp::Ge, 1}
    };
    CHECK(!rules.EvaluateAll(bad_conditions, bad_facts));

    edge::event::EventEngine events(32);

    contracts::event::EventCandidate candidate;
    candidate.event_type = "unattended";
    candidate.source_algorithm = "pipeline.unattended";
    candidate.stream_id = 1;
    candidate.started_at_ns = 0;
    candidate.observed_at_ns = 6'000'000'000;
    candidate.confidence = 0.9F;

    const auto emitted = events.TryEmit(
        "unattended:1",
        candidate,
        5'000'000'000);
    CHECK(emitted.has_value());
    CHECK(!emitted->event_id.empty());

    // 冷却期内重复事件被抑制。
    candidate.observed_at_ns = 7'000'000'000;
    CHECK(!events.TryEmit(
        "unattended:1",
        candidate,
        5'000'000'000).has_value());

    // 时间戳回退视为新时间基，允许重新建立事件基线。
    candidate.observed_at_ns = 100;
    CHECK(events.TryEmit(
        "unattended:1",
        candidate,
        5'000'000'000).has_value());

    // 多线程唯一 key 压力测试，状态表必须保持在上限以内。
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker) {
        workers.emplace_back([worker, &events] {
            for (int i = 0; i < 100; ++i) {
                contracts::event::EventCandidate item;
                item.event_type = "stress";
                item.source_algorithm = "test";
                item.observed_at_ns =
                    static_cast<std::int64_t>(worker * 1000 + i);
                events.TryEmit(
                    "stress:" + std::to_string(worker) + ":" + std::to_string(i),
                    std::move(item),
                    0);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    CHECK(events.TrackedKeyCount() <= 32);
    events.Clear();
    CHECK(events.TrackedKeyCount() == 0);
}

} // namespace

int main(int argc, char** argv) {
    try {
        // 支持命令行按模块单独执行，便于 CI（持续集成）和死锁/性能问题快速定位。
        const std::string selected = argc > 1 ? argv[1] : "all";

        const auto run = [&](const char* name, const char* label, const auto& fn) {
            if (selected == "all" || selected == name) {
                std::cerr << "[TEST] " << label << std::endl;
                fn();
            }
        };

        // 所有测试对象均在函数局部作用域内销毁，便于人工和动态工具检查资源生命周期。
        run("buffer", "BufferPool（缓冲池）", TestBufferPool);
        run("broker", "FrameBroker（视频帧分发器）", TestFrameBroker);
        run("graph", "GraphExecutor（DAG 执行器）", TestGraphExecutor);
        run("scheduler", "ExecutionScheduler（执行调度器）", TestScheduler);
        run("tracking", "Tracking + Spatial（跟踪与空间）", TestTrackingAndSpatial);
        run("event", "Duration + Rule + Event（时序、规则与事件）", TestDurationRuleAndEvent);

        std::cout << "VisionAI core stability tests PASS\n";
        return 0;
    } catch (const std::exception& ex) {
        // 将测试失败转换为明确退出码，避免未捕获异常触发调试器或让 CI 误判为超时。
        std::cerr << "[FAIL] " << ex.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "[FAIL] 未知异常" << std::endl;
        return 3;
    }
}
