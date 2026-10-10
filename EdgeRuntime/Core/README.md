# EdgeRuntime/Core（边缘运行核心）

本目录保存与具体 AI 模型、媒体 SDK 和业务场景无关的实时运行基础设施。

当前生产基线包括：

- BoundedQueue（有界队列）：所有异步任务排队必须有明确容量。
- LifecycleController（生命周期控制器）：统一 Created → Starting → Running → Stopping → Stopped / Failed 状态。
- ThreadPool（有界线程池）：固定工作线程、固定队列容量、异常隔离、优雅停止和指标快照。

核心原则：

1. 禁止无界队列和无限线程创建。
2. 析构必须能够回收所有工作线程。
3. 第三方插件任务异常不得越过 Executor（执行器）边界。
4. 实时数据路径优先 TryPush（立即尝试），满载时由上层选择丢帧、降频或降级，不允许默认永久阻塞。
5. 正常停机使用 drain（排空）语义；故障紧急停机才允许丢弃待处理任务。
