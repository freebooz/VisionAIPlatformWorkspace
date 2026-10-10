#pragma once

#include "Core/Lifecycle.hpp"
#include "Host/IRuntimeComponent.hpp"
#include "Observability/HealthRegistry.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace visionai::edge::host {

// RuntimeHost（边缘运行宿主）统一管理平台组件的启动、失败回滚、健康状态和逆序停机。
//
// 生产语义：
// 1. AddComponent（注册组件）只能在 Created（已创建）状态调用。
// 2. StartAll（启动全部）按注册顺序启动；任一组件失败时立即逆序回滚已启动组件。
// 3. StopAll（停止全部）按注册顺序的逆序关闭，符合“上层先停、底层后停”的依赖释放原则。
// 4. 析构会执行 StopAll，防止宿主退出时遗漏资源回收。
class RuntimeHost {
public:
    explicit RuntimeHost(std::size_t max_components = 128);
    ~RuntimeHost() noexcept;

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;
    RuntimeHost(RuntimeHost&&) = delete;
    RuntimeHost& operator=(RuntimeHost&&) = delete;

    bool AddComponent(std::unique_ptr<IRuntimeComponent> component);

    bool StartAll(std::string* error = nullptr);

    void StopAll() noexcept;

    core::LifecycleState State() const noexcept;

    const observability::HealthRegistry& Health() const noexcept;

    std::size_t ComponentCount() const;

private:
    void RollbackStarted(std::size_t started_count) noexcept;

    const std::size_t max_components_;

    mutable std::mutex mutex_;
    core::LifecycleController lifecycle_;
    observability::HealthRegistry health_;
    std::vector<std::unique_ptr<IRuntimeComponent>> components_;
    std::size_t started_count_{0};
};

} // namespace visionai::edge::host
