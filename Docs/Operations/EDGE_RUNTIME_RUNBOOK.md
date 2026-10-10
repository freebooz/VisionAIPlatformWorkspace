# Edge Runtime（边缘运行时）运维手册

## 一、启动检查

启动后至少确认：

1. Lifecycle（生命周期）进入 Running。
2. HealthRegistry 核心组件为 Healthy 或允许的 Degraded。
3. FramePool 可用块数量高于安全水位。
4. ThreadPool queue depth 处于低水位。
5. 模型、规则、Pipeline 版本与中心下发一致。
6. 摄像机时间戳持续推进。

## 二、队列过载

出现 QueueFull 或 Timeout 时按以下顺序降级：

    降低非关键算法 FPS
    → 跳过非关键帧
    → 暂停 L2 语义模型
    → 暂停 L1 非关键能力
    → 保留检测 / 跟踪 / 证据核心链

禁止把队列临时改为无界，也禁止增加无限线程。

## 三、算法异常

单个算法异常时：

1. failed 指标递增。
2. Health 状态进入 Degraded。
3. 连续失败达到阈值后隔离算法。
4. Pipeline 根据 fallback 选择替代能力。
5. 保留模型版本、算法版本和输入样本索引。

## 四、视频断流

标准处理：

    检测超时
    → 标记视频 Degraded
    → 关闭旧连接
    → 指数退避重连
    → 恢复后重置时间基相关 Scheduler / Duration 状态
    → 标记 Healthy

禁止高速无限重连压垮摄像机或 NVR。

## 五、优雅停机

正常停机顺序：

    停止新配置
    → 停止新任务提交
    → 关闭媒体输入
    → drain AI 队列
    → 完成必要证据
    → 停止工作线程
    → 卸载 Runtime / 模型
    → 上报最后状态
    → 退出

超过停机超时时才进入 emergency stop（紧急停止），并记录被丢弃任务数量。

## 六、内存增长排查

同时观察：

- RSS / Private Bytes
- FramePool InUse
- ThreadPool queued
- Event tracked keys
- Duration tracked keys
- GPU VRAM / NPU Memory
- 线程数和句柄数

若 RSS 增长但业务状态均稳定，再使用 ASAN、heap profiler（堆分析器）或厂商 GPU/NPU 工具定位。

## 七、发布前检查

必须通过：

- strict CMake build
- Debug / Release CTest
- architecture_guard.py
- comment_guard.py
- model_manifest_check.py
- 目标硬件兼容性测试
- Soak Test（浸泡测试）
