# MediaAdapters（媒体基础设施适配层）

本目录只负责具体媒体技术和厂商接口适配，不负责任何业务规则。

规划：

- `RTSP/（实时流协议适配）`
- `GB28181/（国标视频接入）`
- `ONVIF/（网络视频接口适配）`
- `FFmpeg/（通用媒体库适配）`
- `GStreamer/（流媒体管线适配）`
- `Nvidia/（NVDEC / DeepStream 适配）`
- `Intel/（oneVPL / VAAPI 适配）`
- `Rockchip/（MPP / RGA 适配）`

依赖方向：

```text
EdgeRuntime/Media（媒体编排）
        ↓
Foundation/SDK/Media（媒体 Port）
        ↑
MediaAdapters（媒体 Adapter）
```

禁止算法包直接依赖本目录中的具体实现。

