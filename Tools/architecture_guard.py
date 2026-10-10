from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def iter_source_files(root: Path):
    """遍历需要执行架构依赖检查的 C/C++ 源码文件。"""
    for suffix in ("*.hpp", "*.h", "*.cpp", "*.cc", "*.cxx"):
        yield from root.rglob(suffix)


def find_forbidden(path: Path, forbidden: tuple[str, ...]) -> list[str]:
    """返回文件中命中的禁止依赖标记，用于形成明确的人工审阅报告。"""
    text = path.read_text(encoding="utf-8")
    return [token for token in forbidden if token in text]


def main() -> int:
    """执行平台分层依赖守卫，发现反向依赖时以非零退出码阻断 CI（持续集成）。"""
    failures: list[str] = []

    deprecated = (
        "RuntimeAdapters/Common/IModelRuntime.hpp",
        "RuntimeAdapters/Common/TensorView.hpp",
        "AlgorithmSDK/Cpp/IAlgorithm.hpp",
    )

    for path in iter_source_files(ROOT):
        relative = path.relative_to(ROOT).as_posix()
        text = path.read_text(encoding="utf-8")

        for token in deprecated:
            if token in text:
                failures.append(f"{relative}: 仍引用已迁移接口 {token}")

        if relative.startswith("EdgeRuntime/"):
            bad = find_forbidden(path, ('#include "Packages/', '#include "Products/'))
            for token in bad:
                failures.append(f"{relative}: EdgeRuntime 不允许反向依赖 {token}")

        if relative.startswith("Packages/Algorithms/"):
            bad = find_forbidden(
                path,
                (
                    '#include "EdgeRuntime/',
                    '#include "RuntimeAdapters/',
                    '#include "MediaAdapters/',
                    '#include "Products/',
                ),
            )
            for token in bad:
                failures.append(f"{relative}: Algorithm Package 不允许依赖 {token}")

        if relative.startswith("Contracts/"):
            bad = find_forbidden(
                path,
                (
                    '#include "EdgeRuntime/',
                    '#include "RuntimeAdapters/',
                    '#include "Packages/',
                    '#include "Products/',
                    '#include "Foundation/',
                ),
            )
            for token in bad:
                failures.append(f"{relative}: Contracts 不允许依赖上层 {token}")

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    forbidden_cmake = (
        "target_include_directories(visionai_core",
        "add_library(visionai_core STATIC",
    )
    for token in forbidden_cmake:
        if token in cmake:
            failures.append(f"CMakeLists.txt: 仍存在旧巨型核心库定义 {token}")

    if failures:
        print("Architecture Guard FAIL")
        for item in failures:
            print(f"- {item}")
        return 1

    print("Architecture Guard PASS")
    print("- Contracts 未发现反向依赖")
    print("- EdgeRuntime 未发现对 Packages/Products 的反向依赖")
    print("- Algorithm Packages 未发现对 EdgeRuntime/Adapters/Products 的直接依赖")
    print("- 已移除旧 visionai_core 巨型静态库定义")
    print("- 已移除废弃 RuntimeAdapters/Common 与 AlgorithmSDK/Cpp 接口引用")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
