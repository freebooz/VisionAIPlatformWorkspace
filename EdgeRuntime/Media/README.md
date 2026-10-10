# EdgeRuntime/Media（边缘媒体编排）

本目录属于 Edge Runtime（边缘运行时），负责媒体生命周期和实时数据编排，而不是 FFmpeg、GB/T 28181、NVDEC 等具体 SDK 实现。

职责：

- StreamManager（视频流生命周期管理）
- ReconnectPolicy（断流重连策略）
- DecoderSelection（解码器选择）
- FrameRouting（FrameRef 视频帧引用分发）
- EvidenceBuffer（事件证据环形缓存）

具体媒体实现统一放入 `MediaAdapters/（媒体基础设施适配层）`。

禁止：

- Algorithm Package（算法包）自行建立 RTSP 连接。
- Algorithm Package 自行创建重复 Decoder（解码器）。
- Media Adapter 包含业务场景和规则逻辑。
