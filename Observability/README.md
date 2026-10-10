# Observability（可观测体系）

统一使用 OpenTelemetry（开放遥测）采集 Trace、Metric、Log。

当前边缘运行时已经提供 HealthRegistry（健康注册表）和 ThreadPoolMetrics（线程池指标快照），作为后续 Prometheus / OpenTelemetry Exporter（导出器）的稳定输入。

首期重点指标：

- 视频流在线率、重连次数、帧率和时间戳。
- BufferPool（缓冲池）可用块数量。
- ThreadPool（线程池）submitted/completed/failed/rejected/active/queued。
- BoundedQueue（有界队列）容量、深度、QueueFull/Timeout 次数。
- 算法节点 P50/P95/P99 延迟。
- GPU/NPU/CPU 使用率。
- 每路每天事件数、假警数。
- 模型版本、规则版本、场景版本。

健康状态只保留最新快照，不在进程内无限保存历史时间序列。

推荐健康组件命名：

- media.rtsp.camera-001
- media.decoder.camera-001
- runtime.tensorrt.gpu-0
- algorithm.person-detector
- pipeline.unattended.camera-001
- evidence.camera-001
- control.edge-agent
