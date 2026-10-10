# VisionAIPlatformWorkspace 代码性能与稳定性审查报告

日期：2026-09-28  
范围：当前项目自有 C/C++、Go、Python、TypeScript、Vue 源码。  
不包含：ThirdParty（第三方源码）、node_modules（前端依赖）、.artifacts（构建产物）、尚未接入的真实 ONNX Runtime / TensorRT / OpenVINO / RKNN SDK。

## 一、结论

本轮已对当前实际源码进行性能、资源生命周期、并发、异常边界、状态容量和人工可审阅性检查，并对发现的问题进行增量修复。

当前可以确认：

- C++ Debug（调试）构建和自动测试通过。
- C++ Release（发布）构建和自动测试通过。
- MSVC /analyze（微软静态分析）构建通过，当前输出未发现静态分析告警。
- Architecture Guard（架构守卫）通过。
- Model Package（模型包）清单检查通过。
- Chinese Comment Guard（中文注释守卫）通过，当前 46 个项目自有源文件均至少包含中文注释。
- 自有源码中未发现直接使用 new/delete/malloc/calloc/realloc/free 的手工内存管理代码。
- 核心状态表、事件缓存和订阅集合已经增加容量边界，降低长期运行内存持续增长风险。

不能据此宣称“绝对没有任何内存泄漏”。当前 Runner 上 MSVC AddressSanitizer（地址消毒器）能够成功编译和链接，但 ASAN 插桩后的可执行程序在该 Runner 环境中启动后无输出并超时，因此动态 ASAN 运行结果不可用。需要在正式 Windows 开发机或 CI Agent（持续集成代理）补做 ASAN / Application Verifier / 长时间 Soak Test（浸泡测试）。

## 二、本轮发现并修复的重要问题

### 1. GraphExecutor（DAG 算法图执行器）存在真实正确性风险

原 AddNode（添加节点）代码在同一个 emplace 参数列表中同时使用 id 和 std::move(id)。C++ 参数求值顺序不能保证 map key 一定在 move 前取得值，测试中实际触发执行顺序异常。

修复：

- 先构造 Node，再使用未移动的 id 作为 map key。
- 首次 Execute 时编译并冻结拓扑计划。
- 后续执行直接复用拓扑计划，避免每次执行重复扫描所有节点。
- 节点异常统一捕获并转换为错误，不允许异常穿透实时执行边界。
- 重复依赖、缺失依赖和循环依赖均提前拒绝。

性能变化：

- 原执行阶段接近 O(V²) 的重复扫描。
- 当前编译后单次执行为 O(V)，V 为节点数量。

### 2. FrameBroker（视频帧分发器）原实现每帧需要加锁并复制回调

原 Publish（发布）流程：

- 获取 mutex（互斥锁）。
- 将全部 std::function 回调复制到临时 vector。
- 解锁后逐个调用。

高帧率、多摄像机情况下会形成锁竞争和小对象分配。

修复：

- 使用 atomic shared_ptr（原子共享指针）保存不可变订阅快照。
- Subscribe / Unsubscribe 属于低频控制面操作，才重建快照。
- Publish 热路径无 mutex，也不逐帧复制 std::function。
- 单个订阅者异常被隔离，不影响其他消费者。
- 增加 FailedCallbackCount（失败回调计数）。
- 增加最大订阅数量，避免错误重复订阅长期持有回调闭包及其资源。

### 3. BufferPool（缓冲池）资源生命周期和边界增强

已确认：

- 大块数据缓冲在构造时预分配。
- 运行期 Acquire 不重新分配视频像素块。
- SharedBuffer / FrameRef 通过 shared_ptr 保证最后一个消费者释放后才归还块。
- BufferPool 对象先析构时，只要仍有租约，内部 State 和数据块仍然保持有效。

新增：

- 总容量乘法溢出检查。
- InUse（使用中数量）指标。
- 多线程压力测试。
- 归还路径保持 noexcept 语义。

当前仍存在一个已知的微小性能成本：每次 Acquire 会创建 shared_ptr 控制块，可能产生小对象堆分配。相比整帧像素分配已经大幅降低成本。后续高路数压测若证明该点成为热点，可演进为 Intrusive Reference Count（侵入式引用计数）或固定租约池，但当前优先保证生命周期模型简单可靠。

### 4. TensorView（张量视图）异步生命周期风险

原 TensorView 只有 data/native_handle，没有所有权锚点。未来真实异步 Runtime 若直接保存 TensorView，有 Use-After-Free（释放后使用）风险。

修复：

- TensorView 增加 std::shared_ptr<void> owner。
- 明确要求异步 Runtime 复制 TensorView 并持有 owner 到回调结束。
- FrameRef 同样补充生命周期中文说明。

### 5. ONNX Runtime Adapter（ONNX Runtime 适配器）状态并发

原 loaded_ / model_ 没有并发保护，未来 Model Hot Reload（模型热加载）与 Infer（推理）同时执行可能形成 Data Race（数据竞争）。

修复：

- 增加 shared_mutex（共享读写锁）。
- Load 使用写锁。
- Warmup / Infer 使用读锁。

真实 ONNX Runtime SDK 尚未接入，因此 Session 级并发策略需要在正式接入时继续验证。

### 6. ExecutionScheduler（执行调度器）状态无界和时间回退问题

原状态表可以随着动态 key 无限增长；同时视频重连后时间戳回退可能导致算法长时间不再运行。

修复：

- 默认最多跟踪 4096 个 key，可配置。
- 容量满时淘汰最旧状态。
- 时间戳回退自动重建调度基线。
- NaN / Inf FPS 配置拒绝。
- int64 时间加法使用饱和处理，避免溢出。
- 增加 ResetAll / StateCount。

### 7. EventEngine（事件引擎）线程安全和无界状态

原实现：

- 无 mutex，并发调用存在 Data Race。
- last_emit_ns_ 可以随动态事件 key 无限增长。
- 时间差直接做 int64 减法，极端输入存在溢出风险。

修复：

- 增加 mutex。
- 默认最大跟踪 4096 个 key。
- 容量满时淘汰最旧事件状态。
- 使用 long double 计算时间差避免整数减法溢出。
- 处理时间戳回退。
- 负 cooldown（冷却时间）按 0 处理。
- 增加 Reset / Clear / TrackedKeyCount。
- 增加多线程压力测试。

### 8. DurationTracker（持续时间跟踪器）状态无界和负时长

修复：

- 增加线程安全。
- 最大状态容量默认 4096。
- 时间戳回退重新开始计时。
- 不再产生负持续时间。
- 增加 ResetAll / TrackedKeyCount。
- 容量满时淘汰最旧状态。

### 9. SimpleIoUTracker（基础 IoU 跟踪器）输入健壮性

修复：

- IoU 阈值限制在 [0,1]。
- NaN / Inf / 退化 BoundingBox（目标框）直接过滤。
- age / missed 采用饱和递增，避免 uint32 回绕。
- 增加 Reset / ActiveTrackCount。
- vector 提前 reserve，降低扩容。
- Update 互斥保护，避免同一跟踪器被多个线程同时修改。

性能边界仍然明确：当前匹配复杂度约 O(T×D)，仅作为工程基线。高密度多人场景应使用 ByteTrack + Kalman + Hungarian 等正式跟踪器。

### 10. ZoneMembership / RuleEngine（区域与规则）

ZoneMembership：

- 构造阶段校验多边形点是否有限值。
- 无效多边形稳定返回 outside，不在实时线程抛异常。
- 删除人为 1e-6 分母修补，使用标准射线法跨边条件。

RuleEngine：

- NaN / Inf 数值不允许穿透比较规则。
- 缺失字段继续按规则失败处理。

## 三、ControlPlane（中心控制平面）稳定性加固

源码已完成以下修改，但当前 Runner 没有 Go 工具链，因此尚未完成编译验证。

### HTTP Server

- 不再使用无超时的默认 ListenAndServe。
- ReadHeaderTimeout = 5 秒。
- ReadTimeout = 15 秒。
- WriteTimeout = 30 秒。
- IdleTimeout = 60 秒。
- MaxHeaderBytes = 1 MiB。
- SIGTERM / Ctrl+C 优雅停机，最大等待 10 秒。

### HTTP 输入

- 请求体最大 1 MiB。
- DisallowUnknownFields（拒绝未知字段）。
- 拒绝一个请求体中追加第二个 JSON 值。
- 增加 nosniff 响应头。

### 内存仓储

- 边缘节点默认最多 10,000 个。
- 内存事件默认最多 100,000 条。
- Event 使用固定容量 Ring Buffer（环形缓冲），容量满后覆盖最旧事件，不再无限 append。
- Context（上下文）取消会被尊重。
- 节点列表按 ID 排序，保证结果确定性。

### 应用输入

- 节点 ID / 名称 / 状态和事件 ID / 类型都有长度限制。
- 启动阶段依赖为空直接 fail-fast（快速失败），避免运行中 nil panic。

## 四、中文注释人工审阅基线

新增：

`Tools/comment_guard.py（中文注释守卫）`

当前检查结果：

```text
Chinese Comment Guard PASS: 46 files
```

检查范围：

- C/C++：.h / .hpp / .cpp / .cc / .cxx
- Go：.go
- Python：.py
- TypeScript：.ts
- Vue：.vue

排除：

- .artifacts
- node_modules
- ThirdParty

JSON / YAML 等数据配置格式不强制使用注释，以免破坏格式或引入不兼容解析器。

## 五、验证结果

### Debug

```text
cmake --build --preset dev
PASS

ctest --preset dev
PASS

1/1 tests passed
```

### Release

```text
cmake --build --preset release
PASS

ctest --preset release
PASS

1/1 tests passed
```

测试使用自定义 CHECK，而不是标准 assert，因此 Release / NDEBUG 下仍会真正执行所有检查。

### MSVC Static Analysis

```text
cmake --build --preset analyze
PASS
```

当前构建输出未出现 /analyze 静态分析告警。

### AddressSanitizer

```text
cmake --preset asan
PASS

cmake --build --preset asan
PASS
```

但是：

```text
ctest --preset asan
TIMEOUT
```

进一步按 buffer / broker / graph / scheduler / tracking / event 单独运行 ASAN 插桩程序也都在启动阶段无输出后超时，因此不能将其解释为某个具体模块的内存错误，更可能是当前 WebCodex Windows Runner 的 MSVC ASAN 运行环境问题。

结论：ASAN 编译链已建立，但动态 ASAN 结果仍需在正式开发机或 CI Agent 上补测。

### 其他门禁

```text
Architecture Guard PASS
Model Package manifest check PASS
Chinese Comment Guard PASS
```

## 六、当前内存泄漏判断

静态代码审查结果：

- 项目自有源码没有直接 new/delete/malloc/calloc/realloc/free。
- 核心资源使用 RAII（资源获取即初始化）、vector、string、shared_ptr、unique_ptr、mutex 等标准生命周期对象。
- BufferPool 租约有明确归还路径。
- FrameBroker 订阅数量有边界。
- Scheduler / Duration / Event 状态表有容量边界。
- Go 内存事件缓存已从无限 append 改为固定环形缓冲。

因此当前没有发现明确、可复现的代码级内存泄漏点。

但由于动态 ASAN 在本 Runner 无法执行，不能给出“绝对无内存泄漏”的结论。生产前仍必须进行：

1. Windows / Linux ASAN 或同等级动态内存检查。
2. 24～72 小时多路 RTSP Soak Test（浸泡测试）。
3. 持续记录 RSS / Private Bytes / GPU VRAM / NPU Memory。
4. 重复断流重连、模型热切换、Pipeline 重载。
5. 高并发事件与证据片段生成。
6. 真实 TensorRT / OpenVINO / RKNN Runtime 的资源创建销毁循环。

## 七、仍需重点关注的性能边界

1. SimpleIoUTracker 是 O(T×D)，不能作为高密度正式跟踪算法。
2. EventEngine / DurationTracker 当前使用单 mutex，事件频率不高时合适；若未来单进程百万级 key，应分片。
3. BufferPool 每次 Acquire 仍有 shared_ptr 控制块小对象分配；需要性能基准后再决定是否引入侵入式引用计数。
4. GraphExecutor 当前顺序执行 DAG；后续应对无依赖节点增加线程池并行，但必须先建立 ResourceScheduler（资源调度）和设备上下文约束。
5. 当前没有真实模型和真实视频流，因此不能用当前单元测试结果代表最终 AI Pipeline 的 FPS / P95 / P99。
6. ControlPlane 当前 Memory Adapter 只适合开发和测试，生产必须替换 PostgreSQL / Redis / 对象存储等持久化实现。

## 八、下一阶段验收建议

将以下项目作为正式性能稳定性门禁：

- 每个 Runtime Adapter 必须完成 10,000 次 Load / Infer / Unload 循环测试。
- 单视频流连续运行 24 小时。
- 16 / 32 / 64 路并发长稳测试。
- 断流/重连 1,000 次。
- Pipeline 热更新 1,000 次。
- 模型热切换 100 次。
- CPU / GPU / NPU 内存曲线不能持续单向增长。
- 事件吞吐压力下不能产生无界队列。
- 所有新增自有源文件必须通过 comment_guard.py。
- 所有跨层修改必须通过 architecture_guard.py。
