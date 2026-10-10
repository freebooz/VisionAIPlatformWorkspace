#pragma once

#include "Runtime/TensorView.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace visionai::runtime {

// ModelDescriptor（模型描述）只保存模型逻辑标识与部署制品位置。
struct ModelDescriptor {
    std::string model_id;
    std::string version;
    std::string artifact_path;
};

// TensorOutput（张量输出）是不同推理后端共同使用的最小输出结构。
struct TensorOutput {
    std::string name;
    std::vector<std::int64_t> shape;
    std::vector<float> float32;
    std::vector<std::int64_t> int64;
};

struct InferenceResult {
    bool ok{};
    std::string error;
    std::vector<TensorOutput> outputs;
};

// InferenceCallback（推理完成回调）用于异步推理通知。
// 回调执行期间，具体 Runtime（运行时）实现必须保证输出对象以及输入 TensorView.owner 仍然有效。
using InferenceCallback = std::function<void(InferenceResult)>;

// IModelRuntime（模型运行时接口）属于稳定 Port，不属于任何具体 Adapter。
class IModelRuntime {
public:
    virtual ~IModelRuntime() = default;

    virtual bool Load(const ModelDescriptor& model) = 0;
    virtual bool Warmup() = 0;
    virtual InferenceResult Infer(const TensorView& input) = 0;

    // InferAsync（异步推理）默认退化为同步执行。
    // 具体异步实现必须复制 TensorView 并持有其中的 owner，避免异步任务访问已释放内存。
    virtual void InferAsync(const TensorView& input, InferenceCallback callback) {
        callback(Infer(input));
    }

    virtual const char* Name() const noexcept = 0;
};

using ModelRuntimePtr = std::shared_ptr<IModelRuntime>;

} // namespace visionai::runtime

