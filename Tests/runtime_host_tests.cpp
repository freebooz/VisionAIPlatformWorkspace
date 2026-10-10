#include "Core/Lifecycle.hpp"
#include "Host/IRuntimeComponent.hpp"
#include "Host/RuntimeHost.hpp"
#include "Observability/HealthRegistry.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

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

// FakeComponent（测试组件）用于验证启动顺序、失败回滚和逆序停止。
class FakeComponent final
    : public visionai::edge::host::IRuntimeComponent {
public:
    FakeComponent(
        std::string name,
        bool start_success,
        std::vector<std::string>& trace)
        : name_(std::move(name)),
          start_success_(start_success),
          trace_(trace) {}

    std::string_view Name() const noexcept override {
        return name_;
    }

    bool Start(std::string& error) override {
        trace_.push_back("start:" + name_);

        if (!start_success_) {
            error = "模拟启动失败";
            return false;
        }

        running_ = true;
        return true;
    }

    void Stop() noexcept override {
        if (!running_) {
            return;
        }

        trace_.push_back("stop:" + name_);
        running_ = false;
    }

private:
    std::string name_;
    bool start_success_{true};
    bool running_{false};
    std::vector<std::string>& trace_;
};

void TestSuccessfulLifecycle() {
    using namespace visionai::edge;

    std::vector<std::string> trace;
    host::RuntimeHost runtime(4);

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "media",
                true,
                trace)));

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "runtime",
                true,
                trace)));

    CHECK(runtime.ComponentCount() == 2);

    std::string error;
    CHECK(runtime.StartAll(&error));
    CHECK(error.empty());
    CHECK(runtime.State() == core::LifecycleState::Running);

    const auto health = runtime.Health().Snapshot();
    CHECK(health.size() == 2);
    CHECK(
        runtime.Health().OverallStatus() ==
        observability::HealthStatus::Healthy);

    runtime.StopAll();

    CHECK(runtime.State() == core::LifecycleState::Stopped);
    CHECK(trace.size() == 4);
    CHECK(trace[0] == "start:media");
    CHECK(trace[1] == "start:runtime");
    CHECK(trace[2] == "stop:runtime");
    CHECK(trace[3] == "stop:media");
}

void TestStartupRollback() {
    using namespace visionai::edge;

    std::vector<std::string> trace;
    host::RuntimeHost runtime(4);

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "media",
                true,
                trace)));

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "broken-runtime",
                false,
                trace)));

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "pipeline",
                true,
                trace)));

    std::string error;
    CHECK(!runtime.StartAll(&error));
    CHECK(!error.empty());
    CHECK(runtime.State() == core::LifecycleState::Failed);

    // pipeline 不应启动；media 必须在失败后被自动回滚。
    CHECK(trace.size() == 3);
    CHECK(trace[0] == "start:media");
    CHECK(trace[1] == "start:broken-runtime");
    CHECK(trace[2] == "stop:media");

    const auto health = runtime.Health().Snapshot();
    CHECK(health.size() == 2);
}

void TestRegistrationGuards() {
    std::vector<std::string> trace;
    visionai::edge::host::RuntimeHost runtime(1);

    CHECK(
        runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "only-one",
                true,
                trace)));

    CHECK(
        !runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "overflow",
                true,
                trace)));

    CHECK(runtime.StartAll());

    CHECK(
        !runtime.AddComponent(
            std::make_unique<FakeComponent>(
                "late",
                true,
                trace)));
}

} // namespace

int main() {
    try {
        TestSuccessfulLifecycle();
        TestStartupRollback();
        TestRegistrationGuards();
        return 0;
    } catch (const std::exception& ex) {
        return ex.what() == nullptr ? 3 : 2;
    } catch (...) {
        return 3;
    }
}
