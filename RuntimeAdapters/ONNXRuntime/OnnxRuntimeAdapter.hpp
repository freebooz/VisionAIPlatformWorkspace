#pragma once

#include "Runtime/IModelRuntime.hpp"

#include <shared_mutex>

namespace visionai::runtime {

// OnnxRuntimeAdapter（ONNX Runtime 适配器）当前仍是无第三方 SDK 的工程占位实现。
// state_mutex_（状态读写锁）保证未来模型热加载与并发 Infer（推理）之间不存在数据竞争。
class OnnxRuntimeAdapter final : public IModelRuntime {
public:
    bool Load(const ModelDescriptor& model) override;
    bool Warmup() override;
    InferenceResult Infer(const TensorView& input) override;
    const char* Name() const noexcept override { return "onnxruntime"; }

private:
    mutable std::shared_mutex state_mutex_;
    ModelDescriptor model_;
    bool loaded_{false};
};

} // namespace visionai::runtime
