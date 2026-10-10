#pragma once

#include <string>

namespace visionai::algorithm {

// AlgorithmContext（算法运行上下文）保存宿主分配的算法标识和版本。
struct AlgorithmContext {
    std::string algorithm_id;
    std::string version;
};

// IAlgorithm（算法包接口）是原子 AI 能力的最小稳定宿主接口。
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    // Initialize（初始化）只允许在算法实例进入运行态前调用。
    virtual bool Initialize(const AlgorithmContext& context) = 0;
    // Id（算法标识）返回值必须在算法实例整个生命周期内保持有效。
    virtual const char* Id() const noexcept = 0;
};

} // namespace visionai::algorithm

