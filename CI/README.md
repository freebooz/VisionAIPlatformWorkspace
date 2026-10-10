# CI（持续集成）

当前工程已经建立统一 Quality Gate（质量门禁），不再由开发者手工拼接检查命令。

本地快速检查：

    python Tools/quality_gate.py --mode quick

完整 C++ 生产门禁：

    python Tools/quality_gate.py --mode full

正式 CI 环境建议强制：

    python Tools/quality_gate.py --mode full --require-go --require-frontend

流水线最低要求：

- C++：strict（严格）构建、Warnings as Errors（警告视为错误）、MSVC /analyze、Debug/Release CTest。
- Go：go test ./...，后续补充 staticcheck（静态检查）。
- Python：架构守卫、模型包守卫、中文注释守卫。
- Frontend：TypeScript/Vue 生产构建。
- Model Build：模型转换、量化、兼容性、Accuracy Check（精度检查）和 Benchmark（性能基准）。
- Release：制品签名、SBOM（软件物料清单）、Model BOM（模型物料清单）、版本冻结和回滚包。

当前 Runner 没有 Go 和 pnpm 工具链，因此 full 模式会明确 SKIP（跳过）；正式 CI 必须使用 require 参数把缺失工具链视为失败。
