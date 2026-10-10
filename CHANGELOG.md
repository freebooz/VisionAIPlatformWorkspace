# CHANGELOG（版本变更记录）

## 0.3.0 - 2026-09-28

- 新增 BoundedQueue（有界队列），统一限制进程内任务队列容量，禁止无界积压。
- 新增 ThreadPool（有界线程池），实现固定工作线程、任务回压、异常隔离、优雅排空和运行指标。
- 新增 LifecycleController（生命周期控制器），统一 Created/Starting/Running/Stopping/Stopped/Failed 状态。
- 新增 HealthRegistry（健康注册表），为边缘 Agent 心跳和 OpenTelemetry/Prometheus 接入提供标准健康快照。
- 新增 visionai_edge_core 和 visionai_edge_observability 独立 CMake Target。
- 新增 runtime_primitives_tests，覆盖有界队列、线程池回压、并发提交、生命周期压力和健康状态容量边界。
- 新增 strict CMake Preset：Warnings as Errors（警告视为错误）+ MSVC /analyze（静态分析）。
- 新增 Tools/quality_gate.py 和 Scripts/quality-gate.ps1 统一质量门禁。
- 新增生产工程基线与 Edge Runtime Runbook（边缘运行手册）。
- 完整 Quality Gate 已验证 Strict/Release 构建与 2/2 CTest 通过；Go 与前端工具链在当前 Runner 缺失时明确跳过。

## 0.2.0 - 2026-09-28

- 完成架构收敛：由目录分层升级为 CMake 编译级依赖分层。
- 取消 `visionai_core` 巨型静态库，拆分 Contracts、SDK、Edge Kernel、Adapter、Algorithm、Rule、Event 等独立 Target。
- 将 `IModelRuntime（模型运行时接口）`、`TensorView（张量视图）`迁移到 `Foundation/SDK/Runtime`。
- 将 `IAlgorithm（算法接口）`迁移到 `Foundation/SDK/Algorithm`。
- 新增 `MediaAdapters（媒体基础设施适配层）`，明确与 `EdgeRuntime/Media（媒体编排）`的边界。
- 新增 `Products（产品领域层）`，将 GenericSafety（通用安全场景）迁入 VisionAISafety（通用安全产品）。
- ControlPlane（中心控制平面）重构为 Domain → Application → Ports → Adapters → Transport 模块化单体。
- 新增 `Tools/architecture_guard.py（架构守卫）`。
- CMake Configure、C++ Build、CTest 和 Model Package 检查通过。
- 当前 Runner 未安装 Go 工具链，因此 ControlPlane 尚未执行 Go 编译验证。
- 完成首轮性能与稳定性审查：修复 GraphExecutor 节点插入求值顺序问题，并缓存拓扑执行计划。
- FrameBroker 改为不可变订阅快照，Publish（发布帧）热路径不再逐帧持锁和复制回调。
- ExecutionScheduler、DurationTracker、EventEngine 增加线程安全、时间戳回退处理和状态容量上限。
- SimpleIoUTracker 增加坏框过滤、计数饱和、线程保护和状态清理能力。
- TensorView 增加 owner（生命周期持有者），为异步推理避免 Use-After-Free（释放后使用）。
- ControlPlane 增加 HTTP 超时、优雅停机、请求体上限、严格 JSON 解码和有限容量内存事件环形缓冲。
- 新增 AddressSanitizer、MSVC /analyze、中文注释门禁配置；ASAN 动态执行受当前 Runner 环境限制，尚未形成通过结论。
- 新增 `Docs/Quality/CODE_PERFORMANCE_STABILITY_REVIEW.md` 记录审查边界与后续长稳验收要求。

## 0.1.0 - 2026-09-28

- 初始化 VisionAIPlatformWorkspace（机器视觉 AI 平台工作空间）。
- 建立 Contracts（契约层）、EdgeRuntime（边缘运行时）、AlgorithmSDK（算法 SDK）、RuntimeAdapters（推理适配层）。
- 建立首批基础算法包：跟踪、区域归属、持续时间。
- 建立无人看管示例 Pipeline（算法管线）、Rule（规则）和 Scene（场景）。
- 建立 Go ControlPlane（中心控制平面）最小服务。
- 建立 CMake / CTest 工程基线。
