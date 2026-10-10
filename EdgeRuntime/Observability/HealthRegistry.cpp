#include "Observability/HealthRegistry.hpp"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace visionai::edge::observability {

HealthRegistry::HealthRegistry(std::size_t max_components)
    : max_components_(max_components) {
    if (max_components_ == 0) {
        throw std::invalid_argument("HealthRegistry max_components 必须大于 0");
    }
}

bool HealthRegistry::Update(
    std::string component,
    HealthStatus status,
    std::string message) {
    if (component.empty()) {
        return false;
    }

    // 防止错误组件把大型日志/异常堆栈直接写入健康表造成内存放大。
    constexpr std::size_t kMaxComponentLength = 128;
    constexpr std::size_t kMaxMessageLength = 1024;

    if (component.size() > kMaxComponentLength) {
        return false;
    }

    if (message.size() > kMaxMessageLength) {
        message.resize(kMaxMessageLength);
    }

    std::unique_lock lock(mutex_);

    const auto existing = components_.find(component);
    if (existing == components_.end() &&
        components_.size() >= max_components_) {
        return false;
    }

    // 先构造健康记录，再使用未移动的 component 作为 map key。
    // 不能在同一个赋值表达式中既以 component 作为 key，又 std::move(component)，
    // 因为表达式求值顺序可能导致空字符串被错误写入哈希表。
    ComponentHealth health{
        .component = component,
        .status = status,
        .message = std::move(message),
        .updated_at_unix_ms = NowUnixMs()
    };

    components_.insert_or_assign(component, std::move(health));

    return true;
}

bool HealthRegistry::Remove(const std::string& component) {
    std::unique_lock lock(mutex_);
    return components_.erase(component) > 0;
}

std::vector<ComponentHealth> HealthRegistry::Snapshot() const {
    std::shared_lock lock(mutex_);

    std::vector<ComponentHealth> result;
    result.reserve(components_.size());

    for (const auto& [_, health] : components_) {
        result.push_back(health);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const ComponentHealth& lhs, const ComponentHealth& rhs) {
            return lhs.component < rhs.component;
        });

    return result;
}

HealthStatus HealthRegistry::OverallStatus() const {
    std::shared_lock lock(mutex_);

    if (components_.empty()) {
        return HealthStatus::Starting;
    }

    auto worst = HealthStatus::Starting;
    int worst_severity = Severity(worst);

    for (const auto& [_, health] : components_) {
        const int severity = Severity(health.status);

        if (severity > worst_severity) {
            worst = health.status;
            worst_severity = severity;
        }
    }

    return worst;
}

std::size_t HealthRegistry::Size() const {
    std::shared_lock lock(mutex_);
    return components_.size();
}

std::int64_t HealthRegistry::NowUnixMs() noexcept {
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
}

int HealthRegistry::Severity(HealthStatus status) noexcept {
    switch (status) {
        case HealthStatus::Starting:
            return 0;
        case HealthStatus::Healthy:
            return 1;
        case HealthStatus::Stopped:
            return 2;
        case HealthStatus::Degraded:
            return 3;
        case HealthStatus::Unhealthy:
            return 4;
    }

    return 4;
}

} // namespace visionai::edge::observability
