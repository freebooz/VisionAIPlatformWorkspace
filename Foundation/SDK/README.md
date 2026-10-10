# Foundation/SDK（稳定接口与开发工具层）

本目录保存跨实现稳定的 Port / SPI（端口 / 服务提供接口）。

约束：

- 只能依赖 Contracts（稳定契约层）和标准库。
- 不得依赖 TensorRT、OpenVINO、RKNN、FFmpeg、OpenCV 等具体实现。
- RuntimeAdapters（推理运行时适配器）和 Algorithm Package（算法包）只能实现或使用这里定义的稳定接口。

