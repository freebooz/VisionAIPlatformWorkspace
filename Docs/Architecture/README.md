# VisionAIPlatform（机器视觉 AI 平台）工程架构

## 一、架构定位

平台采用“Center Control Plane（中心控制平面） + Edge Data Plane（边缘数据平面）”总体形态，并在代码层使用七层逻辑架构。

```text
L7 Applications（应用与交互）
        ↑
L6 Product Domain（产品领域）
        ↑
L5 Orchestration（编排与决策）
        ↑
L4 AI Capabilities（AI 原子能力）
        ↑
L3 Infrastructure Adapters（基础设施适配）
        ↑
L2 Edge Runtime Kernel（边缘实时内核）
        ↑
L1 SDK / Ports（稳定接口）
        ↑
L0 Contracts（稳定契约）
```

该分层描述运行时逻辑架构；ControlPlane、MLOps、DataOps、BuildFarm、Observability、Security、Deploy 和 CI 属于横向支撑体系，不作为运行时“层数”继续叠加。

## 二、L0 Contracts（稳定契约层）

职责：定义跨模块稳定的数据结构和协议。

当前包括：

- Media（媒体契约）
- Capability（AI 能力契约）
- Algorithm（算法包 ABI）
- Event（标准事件）

后续扩展 Model、Pipeline、Rule、Scene、Evidence、Edge、API、Proto 等契约。

约束：不得依赖 FFmpeg、TensorRT、OpenVINO、RKNN、OpenCV、数据库、中间件或具体业务模块。

## 三、L1 SDK / Ports（稳定接口层）

当前统一放置在 `Foundation/SDK/`。

核心接口：

- `Runtime/IModelRuntime.hpp`：模型运行时 Port（端口）。
- `Runtime/TensorView.hpp`：张量内存视图。
- `Algorithm/IAlgorithm.hpp`：算法包宿主接口。
- `Media/`：媒体端口规划。

具体 Adapter（适配器）只能实现这里的接口，抽象接口不再放在 Adapter 目录中。

## 四、L2 Edge Runtime Kernel（边缘实时运行内核）

职责：

- BufferPool / FramePool（内存和帧池）
- FrameBroker（共享视频帧）
- GraphExecutor（DAG 执行）
- Scheduler（调度）
- RuntimeManager（运行时管理）
- AlgorithmHost（算法包宿主）
- ResultCache（中间结果缓存）
- Rule / Event / Evidence（边缘轻量决策）

原则：高性能、少变化、无公安/法院/监所等业务语义。

## 五、L3 Infrastructure Adapters（基础设施适配层）

### RuntimeAdapters（推理运行时适配）

- ONNX Runtime
- TensorRT
- OpenVINO
- RKNN
- sherpa-onnx
- vLLM Remote

### MediaAdapters（媒体适配）

- RTSP
- GB/T 28181
- ONVIF
- FFmpeg
- GStreamer
- NVIDIA NVDEC
- Intel oneVPL / VAAPI
- Rockchip MPP / RGA

EdgeRuntime/Media 负责“何时拉流、如何重连、如何路由和缓存”；MediaAdapters 负责“具体 SDK 怎么实现”。

## 六、L4 AI Capabilities（AI 原子能力层）

位于 `Packages/Algorithms/`。

原则：

- 单一职责。
- 标准输入输出。
- 不理解具体产品业务。
- 不直接拉流。
- 不自行创建独立 Decoder。
- 不直接依赖 TensorRT/OpenVINO/RKNN 等具体实现。

典型能力包括 Detection、Tracking、Pose、Action、ReID、OCR、Audio、Semantic、Spatial、Temporal。

## 七、L5 Orchestration（编排与决策层）

主要由以下概念组成：

- Pipeline Package（算法管线包）
- Rule Package（规则包）
- Event Engine（事件引擎）
- Evidence（证据）
- Fusion（融合）

Pipeline 使用 DAG（有向无环图）组合原子能力，不把模型名称写死到产品业务逻辑中。

## 八、L6 Product Domain（产品领域层）

具体产品位于 `Products/`。

当前首个产品：

```text
Products/
└── VisionAISafety/
    ├── manifest.yaml
    └── Scenes/
        └── GenericSafety/
            └── scene.yaml
```

后续可以扩展：

- VisionAIInterrogation（办案区智能督察）
- VisionAIDetention（留置看护）
- VisionAIPrison（监所）
- VisionAICourt（法院）
- VisionAISafety（通用安全）

产品原则上只增加 Scene、Rule、Pipeline 组合、UI Metadata、配置和验收用例，不反向修改底层平台。

## 九、L7 Applications（应用与交互层）

当前包括：

- Apps/WebConsole（管理控制台）
- ControlPlane 对外 HTTP / gRPC 接口
- 后续第三方业务系统集成适配器

## 十、视频帧共享原则

同一路视频只允许一个媒体接入和解码链路。

```text
Camera / NVR
    ↓
MediaAdapter
    ↓
EdgeRuntime Media Orchestration
    ↓
Decoder
    ↓
FramePool
    ↓
FrameRef
    ↓
FrameBroker
    ├─ Detector
    ├─ Tracker
    ├─ Pose
    ├─ Recorder
    └─ Debugger
```

多个消费者共享 FrameRef（引用），不得把整帧 RGB 图像复制作为模块间协议。

## 十一、CMake 编译边界

当前已取消原 `visionai_core` 巨型静态库。

每个逻辑模块拥有独立 CMake Target，Include（头文件搜索路径）限定到具体模块根目录，不再将整个仓库根目录作为 PUBLIC Include。

这样架构约束由编译系统参与强制，而不是只靠文档约定。

## 十二、ControlPlane（中心控制平面）

采用 Modular Monolith（模块化单体），避免在业务尚未稳定时过早拆分微服务。

```text
transport
   ↓
application
   ↓
domain + ports
   ↑
adapters
```

目录：

```text
ControlPlane/internal/
├── domain/
├── application/
├── ports/
├── adapters/
└── transport/
```

HTTP Transport（传输层）不得直接访问具体数据库实现。

## 十三、自动化架构守卫

`Tools/architecture_guard.py` 用于检查：

- Contracts 反向依赖。
- EdgeRuntime 对 Products/Packages 的反向依赖。
- Algorithm Package 对 EdgeRuntime/Adapters/Products 的直接依赖。
- 废弃接口路径残留。
- 旧 `visionai_core` 巨型静态库残留。

任何后续重构均应把该检查加入 CI（持续集成）。

## 十四、架构演进原则

1. 先稳定 Contract 和 Port，再增加具体 Adapter。
2. 先复用中间结果，再增加新模型。
3. 新模型通过 ModelPackage + RuntimeAdapter 接入，不修改业务场景。
4. 新硬件通过 HardwareProfile + RuntimeAdapter + MediaAdapter 接入，不修改算法管线。
5. 新产品通过 Products 层组合已有能力，不污染 EdgeRuntime。
6. 任何跨层依赖新增都必须通过架构评审。
