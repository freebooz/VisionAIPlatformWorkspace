#include "ONNXRuntime/OnnxRuntimeAdapter.hpp"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace visionai::runtime {

bool OnnxRuntimeAdapter::Load(const ModelDescriptor& model) {
    // Load（加载）属于写操作，禁止与 Warmup / Infer 并发修改内部模型状态。
    std::unique_lock lock(state_mutex_);
    model_ = model;
    loaded_ = !model.artifact_path.empty();
    return loaded_;
}

bool OnnxRuntimeAdapter::Warmup() {
    // 当前占位实现没有真实 Session（推理会话），仅检查模型是否已完成逻辑加载。
    std::shared_lock lock(state_mutex_);
    return loaded_;
}

InferenceResult OnnxRuntimeAdapter::Infer(const TensorView& input) {
    // 真实 ONNX Runtime 接入后，应把 Session 的线程安全策略封装在本适配器内部，
    // 上层 Algorithm Package（算法包）不感知具体运行时实现。
    std::shared_lock lock(state_mutex_);
    if (!loaded_) {
        return {false, "ONNX Runtime 模型尚未加载", {}};
    }
    // 对 Host（主机内存）输入至少要求 data + bytes 有效；
    // 设备内存输入未来可通过 native_handle（原生句柄）走零拷贝路径。
    if (input.data == nullptr || input.bytes == 0) {
        return {false, "输入 Tensor 为空", {}};
    }
    return {false, "当前构建未启用真实 ONNX Runtime SDK", {}};
}

} // namespace visionai::runtime
