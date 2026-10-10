# BUILD_VERIFICATION（构建验证记录）

日期：2026-09-28  
架构基线：V0.3.0 Production Engineering Baseline（生产工程基线）

## 已验证

### CMake 配置

```text
cmake --preset dev
PASS
```

当前 Runner：

- Visual Studio 18 2026
- MSVC 19.51.36248.0
- C++20

### C++ Debug 构建

```text
cmake --build --preset dev
PASS
```

已成功生成独立模块库：

- visionai_algorithm_spatial.lib
- visionai_algorithm_temporal.lib
- visionai_algorithm_tracking.lib
- visionai_edge_core.lib
- visionai_edge_observability.lib
- visionai_edge_host.lib
- visionai_edge_frame.lib
- visionai_edge_graph.lib
- visionai_edge_memory.lib
- visionai_edge_scheduler.lib
- visionai_event_engine.lib
- visionai_rule_engine.lib
- visionai_runtime_onnx.lib
- visionai_core_tests.exe
- visionai_runtime_primitives_tests.exe
- visionai_runtime_host_tests.exe

原 `visionai_core.lib` 巨型静态库已经取消。

### C++ 自动测试

```text
ctest --preset dev
PASS
```

结果：

- 3/3 tests passed
- BufferPool（缓冲池）
- FrameBroker（共享视频帧）
- GraphExecutor（DAG 算法图）
- ExecutionScheduler（执行调度）
- SimpleIoUTracker（基础跟踪）
- ZoneMembership（区域归属）
- DurationTracker（持续时间）
- RuleEngine（规则引擎）
- EventEngine（事件引擎）
- BoundedQueue（有界队列）
- ThreadPool（有界线程池）
- LifecycleController（生命周期控制器）
- HealthRegistry（健康注册表）
- RuntimeHost（运行宿主）

### Model Package（模型包）检查

```text
python Tools/model_manifest_check.py
PASS
```

当前模型包：

- ModelPackages/Detection/RFDETR/manifest.yaml

### Architecture Guard（架构守卫）

```text
python Tools/architecture_guard.py
PASS
```

验证结果：

- Contracts 未发现反向依赖。
- EdgeRuntime 未发现对 Packages / Products 的反向依赖。
- Algorithm Packages 未发现对 EdgeRuntime / Adapters / Products 的直接依赖。
- 已移除旧 `visionai_core` 巨型静态库定义。
- 已移除废弃 RuntimeAdapters/Common 与 AlgorithmSDK/Cpp 接口引用。

### Performance / Stability Audit（性能与稳定性审查）

Debug（调试）构建与测试：

```text
cmake --build --preset dev
PASS

ctest --preset dev
PASS

3/3 tests passed
```

Release（发布）构建与测试：

```text
cmake --build --preset release
PASS

ctest --preset release
PASS

3/3 tests passed
```

MSVC Static Analysis（微软静态分析）：

```text
cmake --build --preset analyze
PASS
```

当前构建输出未发现 /analyze 静态分析告警。

AddressSanitizer（地址消毒器）：

```text
cmake --preset asan
PASS

cmake --build --preset asan
PASS

ctest --preset asan
TIMEOUT
```

ASAN 插桩程序在当前 WebCodex Windows Runner 中启动后无测试输出并超时；按 buffer / broker / graph / scheduler / tracking / event 分模块执行也表现一致。因此当前只能确认 ASAN 编译链可用，不能宣称动态 ASAN 内存检查通过。需要在正式开发机或 CI Agent（持续集成代理）补测。

中文注释门禁：

```text
python Tools/comment_guard.py
Chinese Comment Guard PASS: 58 files
```

静态源码搜索未发现项目自有代码直接使用 new / delete / malloc / calloc / realloc / free。

详细审查报告：

- `Docs/Quality/CODE_PERFORMANCE_STABILITY_REVIEW.md`

### Production Quality Gate（生产质量门禁）

    python Tools/quality_gate.py --mode full
    PASS

完整门禁已验证：Architecture Guard、Model Package Guard、Chinese Comment Guard、Strict Configure/Build/CTest、Release Configure/Build/CTest 全部通过；Strict 和 Release 均为 3/3 CTest 通过。

当前 Runner 缺少 Go 与 pnpm，因此 ControlPlane / WebConsole 在 full 模式中明确标记 SKIP；正式 CI 应使用 --require-go --require-frontend。

## ControlPlane（中心控制平面）

已完成源码级 Modular Monolith（模块化单体）重构：

```text
domain → application → ports ← adapters
                       ↑
                   transport
```

当前 Runner 检测结果：

```text
GO_NOT_FOUND
```

因此本次没有宣称 `go test ./...` 已执行成功。待 Runner 安装 Go 工具链后补做编译和测试验证。

## WebConsole（Web 管理控制台）

本次架构收敛没有安装前端依赖，也没有执行 pnpm build。

## 真实模型 SDK

当前 ONNX Runtime Adapter（ONNX Runtime 适配器）仍是可编译工程占位实现，未绑定真实 ONNX Runtime SDK。

TensorRT、OpenVINO、RKNN 等目录继续作为后续目标硬件适配点。

## 结论

当前工程已经由“单一 visionai_core + 全仓库 PUBLIC Include”的弱边界结构，调整为独立 CMake Target + 分层 Include 路径的编译级架构。

现有 C++ 基础链路保持通过：

```text
共享视频帧
→ DAG
→ 跟踪
→ 区域
→ 持续时间
→ 规则
→ 事件
```
