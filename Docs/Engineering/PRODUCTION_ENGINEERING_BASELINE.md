# VisionAIPlatformWorkspace 生产工程基线

版本：V0.3.0
日期：2026-09-28

## 一、定位

本基线用于把机器视觉 AI 工程从“功能可运行”提升到“可长期研发、可压测、可诊断、可回滚、可稳定运行”的生产研发状态。

当前完成的是生产工程底座，不代表已经完成真实 RTSP、多模型、GPU/NPU、数据库和现场设备的 7×24 最终验收。

## 二、生产代码必须满足的边界

### 1. 内存有上界

禁止无界队列、无界事件缓存、无界动态状态表、每帧重复申请大块图像内存。

当前基线：

- BufferPool（缓冲池）预分配大块帧内存。
- BoundedQueue（有界队列）固定容量。
- EventEngine、DurationTracker、ExecutionScheduler 均有限定状态容量。
- ControlPlane 开发态事件仓储使用固定容量 Ring Buffer（环形缓冲）。

### 2. 并发有上界

禁止每个任务新建永久线程、模型推理无限并发、负载过高时自动扩出无限线程。

当前 ThreadPool（有界线程池）固定 worker（工作线程）数量和 queue_capacity（队列容量）。满载必须明确返回 QueueFull 或 Timeout，由上层实施丢帧、降频、降级或重试。

### 3. 生命周期一致

统一状态：

    Created
    → Starting
    → Running
    → Stopping
    → Stopped

异常状态：

    → Failed

正常停机必须先停止接收新任务，再关闭输入、排空已接受任务、等待工作线程、卸载模型/媒体资源，最后更新健康状态。

### 4. 异常隔离

第三方算法、Runtime Adapter（运行时适配器）和任务回调异常不得穿透边缘运行时主循环。

当前 ThreadPool 和 GraphExecutor 均在边界捕获异常，并形成失败计数或错误结果。

### 5. 运行状态可观测

最低指标：

- queue capacity / queued（队列容量 / 当前积压）
- submitted / completed / failed / rejected（提交 / 完成 / 失败 / 拒绝）
- active workers（活动工作线程）
- FramePool available / in-use（帧池可用 / 使用中）
- 算法 FPS、P50/P95/P99
- 视频流在线率和重连次数
- CPU / GPU / NPU / 内存
- 事件数、抑制数、证据失败数

HealthRegistry（健康注册表）只保存最新状态，历史时间序列交给 OpenTelemetry（开放遥测）和 Prometheus。

### 6. 失败可回归

任何生产缺陷都应尽量形成：

    失败输入
    → 自动化测试
    → 修复
    → 回归门禁

禁止只靠现场人工判断“已经修复”。

## 三、统一回压策略

实时视频帧使用 TrySubmit。队列满时应跳过非关键帧、降低检测 FPS 或关闭低优先级能力，不能阻塞解码线程。

控制任务使用 SubmitFor，并设置有限超时。不能永久等待。

证据和审计任务不能静默丢弃，应使用持久化队列或 Transactional Outbox（事务外发）；当前 BoundedQueue 只承担进程内运行任务。

## 四、ThreadPool 使用规范

每个线程池必须明确：

- worker_count（工作线程数）
- queue_capacity（队列容量）
- 满载策略
- 任务最长执行时间
- 停机是否 drain（排空）
- 监控名称

算法包不得自行创建私有线程池，统一由 EdgeRuntime 宿主管理。

## 五、HealthRegistry 规范

推荐组件命名：

- media.rtsp.camera-001
- media.decoder.camera-001
- runtime.tensorrt.gpu-0
- algorithm.person-detector
- pipeline.unattended.camera-001
- evidence.camera-001
- control.edge-agent

状态统一为 Starting、Healthy、Degraded、Unhealthy、Stopped。

## 六、严格质量门禁

快速检查：

    python Tools/quality_gate.py --mode quick

完整 C++ 门禁：

    python Tools/quality_gate.py --mode full

Windows 包装入口：

    Scripts/quality-gate.ps1 -Mode full

正式 CI 应使用：

    python Tools/quality_gate.py --mode full --require-go --require-frontend

strict（严格构建）开启 W4 或 Wall/Wextra/Wpedantic、Warnings as Errors（警告视为错误）和 MSVC /analyze（微软静态分析）。

## 七、长稳验收

最低要求：

- 单流连续运行 24 小时。
- RTSP 断流/恢复 1000 次。
- Pipeline 热加载 1000 次。
- 模型 Load/Unload 100 次。
- 16 / 32 / 64 路并发。
- 持续采集 RSS、Private Bytes、GPU VRAM、NPU Memory、Handle、Thread Count、Queue Depth、P95、P99。

任何资源曲线持续单向增长均不得发布。

## 八、当前未完成的生产项

后续仍需实现：

1. FFmpeg / GStreamer 原生媒体接入。
2. TensorRT / OpenVINO / RKNN 真实运行时。
3. Runtime Context Pool（推理上下文池）。
4. PreprocessCache（公共预处理缓存）。
5. ByteTrack 正式跟踪算法。
6. OpenTelemetry 真正指标导出。
7. EdgeAgent 心跳、配置下发和 A/B 回滚。
8. PostgreSQL / 对象存储 / 消息总线生产实现。
9. Go ControlPlane 编译验证。
10. WebConsole 生产构建验证。
11. 真实硬件 24～72 小时 Soak Test（浸泡测试）。

## 九、代码评审红线

直接拒绝：

- 无界 std::queue 或 vector 作为永久任务缓存。
- 算法包直接拉 RTSP。
- 算法包直接依赖 TensorRT/RKNN 厂商 API。
- 业务 Scene 反向修改 EdgeRuntime。
- 线程无停止路径。
- 裸 new/delete/malloc/free 所有权不清。
- 捕获异常后完全静默。
- 新源码没有中文注释。
- 编译警告增加但仍允许合并。
