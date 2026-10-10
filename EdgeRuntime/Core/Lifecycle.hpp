#pragma once

#include <atomic>
#include <cstdint>

namespace visionai::edge::core {

// LifecycleState（生命周期状态）统一边缘组件的启动、运行、停止语义。
enum class LifecycleState : std::uint8_t {
    Created = 0,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed
};

// LifecycleController（生命周期控制器）提供无锁状态查询和受控状态迁移。
// 组件不应直接暴露多个 bool（布尔）变量描述启动/停止状态，避免组合状态矛盾。
class LifecycleController {
public:
    LifecycleController() = default;

    LifecycleController(const LifecycleController&) = delete;
    LifecycleController& operator=(const LifecycleController&) = delete;

    LifecycleState State() const noexcept {
        return state_.load(std::memory_order_acquire);
    }

    bool TryStart() noexcept {
        auto expected = LifecycleState::Created;
        return state_.compare_exchange_strong(
            expected,
            LifecycleState::Starting,
            std::memory_order_acq_rel);
    }

    void MarkRunning() noexcept {
        state_.store(LifecycleState::Running, std::memory_order_release);
    }

    bool RequestStop() noexcept {
        auto current = state_.load(std::memory_order_acquire);

        while (current != LifecycleState::Stopped &&
               current != LifecycleState::Failed &&
               current != LifecycleState::Stopping) {
            if (state_.compare_exchange_weak(
                    current,
                    LifecycleState::Stopping,
                    std::memory_order_acq_rel)) {
                return true;
            }
        }

        return current == LifecycleState::Stopping;
    }

    void MarkStopped() noexcept {
        state_.store(LifecycleState::Stopped, std::memory_order_release);
    }

    void MarkFailed() noexcept {
        state_.store(LifecycleState::Failed, std::memory_order_release);
    }

    bool AcceptingWork() const noexcept {
        return State() == LifecycleState::Running;
    }

    bool StopRequested() const noexcept {
        const auto state = State();
        return state == LifecycleState::Stopping ||
               state == LifecycleState::Stopped ||
               state == LifecycleState::Failed;
    }

private:
    std::atomic<LifecycleState> state_{LifecycleState::Created};
};

} // namespace visionai::edge::core
