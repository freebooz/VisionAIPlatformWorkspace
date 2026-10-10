# VisionAIPlatformWorkspace（机器视觉 AI 平台工作空间）

本仓库是“机器视觉 AI 智能监管平台”的产品级工作空间。当前架构已经从“目录分层”收敛为“编译级依赖分层”，核心原则是：稳定契约、稳定 Port / SDK（端口 / 开发接口）、高性能边缘运行内核、可替换基础设施适配器、可插拔 AI 能力、可配置编排和独立产品领域层。

## 当前工程基线

- C++20：边缘实时数据平面、算法运行时和规则/事件引擎。
- Go：ControlPlane（中心控制平面），采用 Modular Monolith（模块化单体）分层。
- Python：模型转换、架构检查、评测与工程工具。
- Vue3 + TypeScript：WebConsole（管理控制台）骨架。
- 大型模型权重和硬件二进制制品不提交源码仓库，只提交 manifest（清单）、配置、版本和哈希。

## 逻辑分层

```text
L7 Applications（应用与交互层）
        ↑
L6 Product Domain（产品领域层）
        ↑
L5 Orchestration（算法编排 / 规则 / 事件 / 证据）
        ↑
L4 AI Capabilities（AI 原子能力层）
        ↑
L3 Infrastructure Adapters（基础设施适配层）
        ↑
L2 Edge Runtime Kernel（边缘实时运行内核）
        ↑
L1 SDK / Ports（稳定接口层）
        ↑
L0 Contracts（稳定契约层）
```

横向支撑体系包括：ControlPlane（中心控制平面）、MLOps（模型工程）、DataOps（数据工程）、BuildFarm（模型构建农场）、Observability（可观测）、Security（安全）、Deploy（部署）和 CI（持续集成）。

## 当前核心目录

- `Contracts/`：稳定数据契约。
- `Foundation/SDK/`：Runtime / Algorithm / Media 等稳定 Port（端口）。
- `EdgeRuntime/`：实时运行内核和边缘编排。
- `RuntimeAdapters/`：ONNX Runtime、TensorRT、OpenVINO、RKNN 等推理适配器。
- `MediaAdapters/`：RTSP、GB/T 28181、ONVIF、FFmpeg、硬件解码等媒体适配器。
- `Packages/Algorithms/`：平台级 AI 原子能力。
- `Packages/Pipelines/`：跨产品可复用算法管线。
- `Packages/Rules/`：跨产品公共规则。
- `Products/`：具体产品领域包。
- `ControlPlane/`：中心控制平台。
- `ModelPackages/`：模型定义和硬件制品清单。
- `Docs/Engineering/`：生产工程基线、研发规范和质量门禁说明。
- `Docs/Operations/`：边缘节点运维 Runbook（运行手册）。

## CMake Target（构建目标）

已取消原单体 `visionai_core` 巨型静态库，拆分为：

- `visionai_contracts（契约接口库）`
- `visionai_runtime_sdk（运行时 SDK）`
- `visionai_algorithm_sdk（算法 SDK）`
- `visionai_edge_core（边缘运行核心：有界队列/线程池/生命周期）`
- `visionai_edge_observability（边缘健康状态）`
- `visionai_edge_host（边缘组件宿主与失败回滚）`
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

并提供 `visionai_edge_kernel`、`visionai_edge_intelligence`、`visionai_platform_algorithms` 聚合 Target。

## 关键架构约束

1. Contracts（契约层）不得依赖模型 SDK、媒体 SDK、数据库或业务模块。
2. Foundation/SDK（稳定接口层）只能依赖 Contracts 和标准库。
3. EdgeRuntime（边缘运行时）不得依赖 Products（产品领域层）。
4. Algorithm Package（算法包）不得自行建立 RTSP 连接、重复解码或直接依赖具体 Runtime Adapter。
5. Runtime Adapter（运行时适配器）必须实现 Foundation/SDK/Runtime 中的稳定接口。
6. 产品领域原则上只能组合 Pipeline、Rule、Scene、UI Metadata 和 Acceptance Cases。
7. 新增行业场景不应修改 EdgeRuntime、Contracts 或具体推理适配器。

## 快速构建

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

python Tools/model_manifest_check.py
python Tools/architecture_guard.py
```

Go 工具链可用时：

```powershell
cd ControlPlane
go test ./...
```

## 当前首期实现

- FrameRef（共享视频帧引用）
- BufferPool（缓冲池）
- FrameBroker（视频帧分发器）
- GraphExecutor（DAG 算法图执行器）
- ExecutionScheduler（执行调度器）
- IModelRuntime（稳定模型运行时接口）
- IAlgorithm（稳定算法包接口）
- SimpleIoUTracker（基础目标跟踪）
- ZoneMembership（区域归属）
- DurationTracker（持续时间）
- RuleEngineLite（轻量规则引擎）
- EventEngineLite（轻量事件引擎）
- BoundedQueue（有界任务队列）
- ThreadPool（有界线程池）
- LifecycleController（生命周期控制器）
- HealthRegistry（健康状态注册表）
- RuntimeHost（运行宿主：统一启动、失败回滚和逆序停机）
- UnattendedDetection（无人看管）示例 Pipeline
- VisionAISafety（通用机器视觉安全产品）首个产品域
- ControlPlane Modular Monolith（中心控制平面模块化单体）骨架

详细架构见 `Docs/Architecture/README.md`，本次架构收敛计划见 `Docs/Architecture/ARCHITECTURE_CONVERGENCE_PLAN.md`。

## 质量门禁

当前工程提供以下自动检查：

```powershell
ctest --preset dev
ctest --preset release
python Tools/architecture_guard.py
python Tools/model_manifest_check.py
python Tools/comment_guard.py
```

内存安全与静态分析构建：

```powershell
cmake --preset asan
cmake --build --preset asan

cmake --preset analyze
cmake --build --preset analyze
```

当前性能、资源生命周期、并发和稳定性审查记录见：

`Docs/Quality/CODE_PERFORMANCE_STABILITY_REVIEW.md（代码性能与稳定性审查报告）`

生产工程基线和边缘运行手册：

- `Docs/Engineering/PRODUCTION_ENGINEERING_BASELINE.md`
- `Docs/Operations/EDGE_RUNTIME_RUNBOOK.md`

统一质量门禁：

    python Tools/quality_gate.py --mode quick
    python Tools/quality_gate.py --mode full
