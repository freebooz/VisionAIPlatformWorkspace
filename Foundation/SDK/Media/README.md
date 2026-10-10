# Foundation/SDK/Media（媒体接口层）

本目录定义 EdgeRuntime（边缘运行时）与 MediaAdapters（媒体适配器）之间的稳定 Port（端口）。

后续接口包括：

- IMediaSource（统一媒体源）
- IEncodedPacketSource（编码包媒体源）
- IVideoDecoder（视频解码器）
- IVideoRemuxer（无重编码重封装器）

约束：接口层只能依赖 Contracts（稳定契约层）和标准库，不能直接依赖 FFmpeg、GStreamer、NVDEC、oneVPL、MPP/RGA 等具体 SDK。
