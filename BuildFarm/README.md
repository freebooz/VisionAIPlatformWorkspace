# VisionAIModelBuildFarm（机器视觉 AI 模型构建农场）

负责将训练模型转换成可部署 Hardware Variant（硬件变体）：

```text
PyTorch / Paddle
      ↓
ONNX
      ↓
Compatibility Check（兼容性检查）
      ↓
FP16 / INT8
      ↓
TensorRT / OpenVINO / RKNN
      ↓
Accuracy Check（精度回归）
      ↓
Benchmark（性能基准）
      ↓
Model Package（模型包）
      ↓
Signature（数字签名）
```

现场边缘节点不得自行从互联网下载或转换未知模型。
