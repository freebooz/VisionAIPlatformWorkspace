# Deploy（部署工程）

规划部署形态：

- Center（中心平台）：Docker Compose / Kubernetes / Helm。
- Edge（边缘节点）：systemd / containerd，按 NVIDIA、Intel、Rockchip 区分 Hardware Variant（硬件变体）。
- Fleet（大规模边缘集群）：规模扩大后可引入 KubeEdge。
- Offline（专网离线）：签名制品离线导入。

首期开发环境优先采用本地 CMake + Go，生产容器化在后续阶段补齐。
