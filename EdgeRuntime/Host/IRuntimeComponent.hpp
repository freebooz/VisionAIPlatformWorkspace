#pragma once

#include <string>
#include <string_view>

namespace visionai::edge::host {

// IRuntimeComponent（运行组件接口）定义边缘宿主统一的启动/停止生命周期边界。
//
// 约束：
// - Name（组件名称）在组件整个生命周期内必须稳定且唯一。
// - Start（启动）失败时返回 false，并通过 error 输出可审阅原因。
// - Stop（停止）必须幂等且 noexcept，禁止异常穿透宿主停机流程。
// - 组件内部线程、模型、文件句柄和设备句柄必须在 Stop 返回前完成回收。
class IRuntimeComponent {
public:
    virtual ~IRuntimeComponent() = default;

    virtual std::string_view Name() const noexcept = 0;

    virtual bool Start(std::string& error) = 0;

    virtual void Stop() noexcept = 0;
};

} // namespace visionai::edge::host
