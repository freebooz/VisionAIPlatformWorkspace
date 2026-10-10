# VisionAIPlatformWorkspace 架构收敛执行计划

版本：V0.2.0-Architecture-Convergence  
日期：2026-09-28

## 一、目标

本次不重建工程，基于现有代码增量调整，使“目录分层”升级为“编译级依赖分层”。

核心目标：

1. 拆分 `visionai_core（平台核心库）`，避免形成巨型静态库。
2. 将 `IModelRuntime（模型运行时接口）` 和 `TensorView（张量视图）` 从适配器目录迁移到稳定 SDK/Port（接口）层。
3. 取消全仓库根目录作为 PUBLIC Include（公共头文件目录），让 CMake Target（构建目标）限定可见范围。
4. 明确 `EdgeRuntime/Media（媒体编排）` 与 `MediaAdapters（媒体适配器）` 的职责边界。
5. 新增 `Products（产品领域层）`，将 GenericSafety（通用安全）从平台包中迁出。
6. 将 Go ControlPlane（中心控制平面）重构为 Modular Monolith（模块化单体）：Domain → Application → Ports → Adapters → Transport。
7. 保持现有 C++ 功能和自动测试通过。

## 二、执行阶段

### 阶段 A：稳定接口层

新增：

- `Foundation/SDK/Runtime/IModelRuntime.hpp`
- `Foundation/SDK/Runtime/TensorView.hpp`
- `Foundation/SDK/Algorithm/IAlgorithm.hpp`

迁移并删除：

- `RuntimeAdapters/Common/IModelRuntime.hpp`
- `RuntimeAdapters/Common/TensorView.hpp`
- `AlgorithmSDK/Cpp/IAlgorithm.hpp`

完成标准：

- Runtime Adapter 只能依赖 Runtime SDK。
- Algorithm Package 只能依赖 Contracts / SDK。

### 阶段 B：CMake 物理依赖拆分

拆分为独立 Target：

- `visionai_contracts（契约接口库）`
- `visionai_runtime_sdk（运行时 SDK）`
- `visionai_algorithm_sdk（算法 SDK）`
- `visionai_edge_memory（边缘内存）`
- `visionai_edge_frame（共享视频帧）`
- `visionai_edge_graph（DAG 执行器）`
- `visionai_edge_scheduler（调度器）`
- `visionai_rule_engine（规则引擎）`
- `visionai_event_engine（事件引擎）`
- `visionai_runtime_onnx（ONNX Runtime 适配器）`
- `visionai_algorithm_tracking（跟踪算法）`
- `visionai_algorithm_spatial（空间算法）`
- `visionai_algorithm_temporal（时序算法）`

增加聚合 Target：

- `visionai_edge_kernel（边缘运行内核）`
- `visionai_edge_intelligence（边缘智能决策）`
- `visionai_platform_algorithms（平台通用算法）`

完成标准：

- 不再存在单一 `visionai_core` 聚合所有代码。
- 不再给业务代码暴露整个仓库根目录作为 PUBLIC Include。

### 阶段 C：媒体职责分离

建立：

- `MediaAdapters/（媒体基础设施适配器）`
- `EdgeRuntime/Media/（边缘媒体编排）`

规则：

- MediaAdapters 负责 FFmpeg、GB/T 28181、ONVIF、NVDEC、oneVPL、MPP/RGA 等具体实现。
- EdgeRuntime/Media 负责流生命周期、重连、解码器选择、FrameRef 分发与证据缓存。

### 阶段 D：产品领域层

新增：

- `Products/VisionAISafety/（通用安全产品）`

迁移：

- `Packages/Scenes/GenericSafety` → `Products/VisionAISafety/Scenes/GenericSafety`

保留：

- `Packages/Algorithms`：平台原子能力。
- `Packages/Pipelines`：跨产品可复用管线。
- `Packages/Rules`：跨产品公共规则。

### 阶段 E：ControlPlane 模块化单体

目标目录：

```text
ControlPlane/internal/
├── domain/
│   ├── edge/
│   └── event/
├── application/
│   └── controlplane/
├── ports/
│   └── repositories/
├── adapters/
│   └── memory/
└── transport/
    └── http/
```

依赖方向：

```text
transport
   ↓
application
   ↓
domain + ports
   ↑
adapters
```

HTTP 层不得直接操作存储实现。

### 阶段 F：架构文档和测试

更新：

- `README.md`
- `Docs/Architecture/README.md`
- `BUILD_VERIFICATION.md`

验证：

- `cmake --preset dev`
- `cmake --build --preset dev`
- `ctest --preset dev`
- `python Tools/model_manifest_check.py`

Go 测试在 Runner 提供 Go 工具链时执行。

## 三、暂不执行的事项

本次不做：

- RF-DETR 真实权重接入。
- TensorRT/OpenVINO/RKNN SDK 实机编译。
- 前端完整业务页面开发。
- 微服务拆分。
- Kubernetes/KubeEdge 正式部署。

这些工作在架构边界稳定后进入后续版本。
