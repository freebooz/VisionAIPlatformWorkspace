#include "Host/RuntimeHost.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace visionai::edge::host {

RuntimeHost::RuntimeHost(std::size_t max_components)
    : max_components_(max_components),
      health_(max_components) {
    if (max_components_ == 0) {
        throw std::invalid_argument("RuntimeHost max_components 必须大于 0");
    }

    components_.reserve(max_components_);
}

RuntimeHost::~RuntimeHost() noexcept {
    StopAll();
}

bool RuntimeHost::AddComponent(
    std::unique_ptr<IRuntimeComponent> component) {
    if (!component) {
        return false;
    }

    std::lock_guard lock(mutex_);

    if (lifecycle_.State() != core::LifecycleState::Created) {
        return false;
    }

    if (components_.size() >= max_components_) {
        return false;
    }

    const auto name = component->Name();
    if (name.empty()) {
        return false;
    }

    // 注册阶段拒绝重名组件，避免健康状态和日志无法区分真实来源。
    for (const auto& existing : components_) {
        if (existing->Name() == name) {
            return false;
        }
    }

    components_.push_back(std::move(component));
    return true;
}

bool RuntimeHost::StartAll(std::string* error) {
    std::lock_guard lock(mutex_);

    if (!lifecycle_.TryStart()) {
        if (error) {
            *error = "RuntimeHost 当前状态不允许启动";
        }
        return false;
    }

    started_count_ = 0;

    for (std::size_t index = 0; index < components_.size(); ++index) {
        auto& component = components_[index];
        const std::string name(component->Name());

        health_.Update(
            name,
            observability::HealthStatus::Starting,
            "组件启动中");

        std::string component_error;
        bool started = false;

        try {
            started = component->Start(component_error);
        } catch (const std::exception& ex) {
            component_error =
                std::string("组件启动抛出异常: ") + ex.what();
        } catch (...) {
            component_error = "组件启动抛出未知异常";
        }

        if (!started) {
            health_.Update(
                name,
                observability::HealthStatus::Unhealthy,
                component_error.empty()
                    ? "组件启动失败"
                    : component_error);

            if (error) {
                *error =
                    "组件启动失败: " +
                    name +
                    (component_error.empty()
                        ? std::string{}
                        : ", " + component_error);
            }

            // 当前失败组件不计入 started_count_，只回滚此前已经成功启动的组件。
            RollbackStarted(started_count_);
            started_count_ = 0;
            lifecycle_.MarkFailed();
            return false;
        }

        ++started_count_;

        health_.Update(
            name,
            observability::HealthStatus::Healthy,
            "组件运行正常");
    }

    lifecycle_.MarkRunning();
    return true;
}

void RuntimeHost::StopAll() noexcept {
    std::lock_guard lock(mutex_);

    const auto state = lifecycle_.State();

    if (state == core::LifecycleState::Stopped ||
        (state == core::LifecycleState::Created &&
         started_count_ == 0)) {
        lifecycle_.MarkStopped();
        return;
    }

    lifecycle_.RequestStop();

    RollbackStarted(started_count_);
    started_count_ = 0;
    lifecycle_.MarkStopped();
}

core::LifecycleState RuntimeHost::State() const noexcept {
    return lifecycle_.State();
}

const observability::HealthRegistry&
RuntimeHost::Health() const noexcept {
    return health_;
}

std::size_t RuntimeHost::ComponentCount() const {
    std::lock_guard lock(mutex_);
    return components_.size();
}

void RuntimeHost::RollbackStarted(
    std::size_t started_count) noexcept {
    // 逆序停止已成功启动的组件，确保依赖上层先释放、底层后释放。
    while (started_count > 0) {
        --started_count;

        auto& component = components_[started_count];
        // 停机路径声明 noexcept，因此连诊断字符串分配失败也不能破坏剩余资源清理。
        std::string name;
        try {
            name.assign(component->Name());
        } catch (...) {
            name.clear();
        }

        // IRuntimeComponent::Stop 明确要求 noexcept；组件实现必须在此返回前完成资源回收。
        component->Stop();

        if (!name.empty()) {
            try {
                health_.Update(
                    name,
                    observability::HealthStatus::Stopped,
                    "组件已停止");
            } catch (...) {
                // 健康状态更新失败不能阻止后续组件清理。
            }
        }
    }
}

} // namespace visionai::edge::host
