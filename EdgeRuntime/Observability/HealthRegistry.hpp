#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace visionai::edge::observability {

// HealthStatus（健康状态）从轻到重排列，OverallStatus（总体状态）取最严重状态。
enum class HealthStatus : std::uint8_t {
    Starting = 0,
    Healthy,
    Degraded,
    Unhealthy,
    Stopped
};

// ComponentHealth（组件健康快照）用于边缘 Agent（代理）心跳和本地诊断。
struct ComponentHealth {
    std::string component;
    HealthStatus status{HealthStatus::Starting};
    std::string message;
    std::int64_t updated_at_unix_ms{};
};

// HealthRegistry（健康状态注册表）保存有限数量组件的最新状态。
// 它只保存“最新快照”，不存历史时间序列，历史指标应交给 OpenTelemetry（开放遥测）/Prometheus。
class HealthRegistry {
public:
    explicit HealthRegistry(std::size_t max_components = 1024);

    bool Update(
        std::string component,
        HealthStatus status,
        std::string message = {});

    bool Remove(const std::string& component);

    std::vector<ComponentHealth> Snapshot() const;

    HealthStatus OverallStatus() const;

    std::size_t Size() const;

private:
    static std::int64_t NowUnixMs() noexcept;
    static int Severity(HealthStatus status) noexcept;

    const std::size_t max_components_;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ComponentHealth> components_;
};

} // namespace visionai::edge::observability
