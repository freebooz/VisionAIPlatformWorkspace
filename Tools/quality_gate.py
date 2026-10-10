from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Step:
    """Quality Gate（质量门禁）中的一个可审计执行步骤。"""

    name: str
    command: tuple[str, ...]
    cwd: Path = ROOT
    required: bool = True


def run_step(step: Step) -> bool:
    """执行单个门禁步骤并把标准输出直接透传，失败时返回 False。"""

    print(f"\n=== {step.name} ===")
    print("$ " + " ".join(step.command))

    try:
        completed = subprocess.run(
            step.command,
            cwd=step.cwd,
            check=False,
        )
    except FileNotFoundError:
        if step.required:
            print(f"[FAIL] 缺少命令: {step.command[0]}")
            return False

        print(f"[SKIP] 可选命令不存在: {step.command[0]}")
        return True

    if completed.returncode != 0:
        if step.required:
            print(
                f"[FAIL] {step.name}，退出码={completed.returncode}"
            )
            return False

        print(
            f"[WARN] 可选步骤失败: {step.name}，"
            f"退出码={completed.returncode}"
        )
        return True

    print(f"[PASS] {step.name}")
    return True


def python_step(name: str, script: str) -> Step:
    """使用当前 Python 解释器执行项目内工具，避免系统 PATH 指向不同版本。"""

    return Step(
        name=name,
        command=(sys.executable, str(ROOT / script)),
    )


def command_available(command: str) -> bool:
    """判断外部工具是否存在，不主动安装任何依赖。"""

    return shutil.which(command) is not None


def build_steps(mode: str) -> list[Step]:
    """根据 quick/full（快速/完整）模式生成确定性的 C++ 质量门禁步骤。"""

    common = [
        python_step(
            "Architecture Guard（架构依赖守卫）",
            "Tools/architecture_guard.py",
        ),
        python_step(
            "Model Package Guard（模型包清单守卫）",
            "Tools/model_manifest_check.py",
        ),
        python_step(
            "Chinese Comment Guard（中文注释守卫）",
            "Tools/comment_guard.py",
        ),
    ]

    if mode == "quick":
        return [
            *common,
            Step(
                "Debug CTest（调试自动测试）",
                ("ctest", "--preset", "dev"),
            ),
        ]

    return [
        *common,
        Step(
            "Strict Configure（严格配置）",
            ("cmake", "--preset", "strict"),
        ),
        Step(
            "Strict Build（警告即错误 + 静态分析构建）",
            ("cmake", "--build", "--preset", "strict"),
        ),
        Step(
            "Strict CTest（严格自动测试）",
            ("ctest", "--preset", "strict"),
        ),
        Step(
            "Release Configure（发布配置）",
            ("cmake", "--preset", "release"),
        ),
        Step(
            "Release Build（发布构建）",
            ("cmake", "--build", "--preset", "release"),
        ),
        Step(
            "Release CTest（发布自动测试）",
            ("ctest", "--preset", "release"),
        ),
    ]


def optional_platform_steps(
    require_go: bool,
    require_frontend: bool,
) -> list[Step]:
    """生成 Go 与前端门禁；当前 Runner 缺工具时可选择跳过或强制失败。"""

    steps: list[Step] = []

    if command_available("go"):
        steps.append(
            Step(
                "Go Test（中心控制平面测试）",
                ("go", "test", "./..."),
                cwd=ROOT / "ControlPlane",
            )
        )
    elif require_go:
        steps.append(
            Step(
                "Go Toolchain（Go 工具链）",
                ("go", "version"),
                required=True,
            )
        )
    else:
        print("[SKIP] Go 工具链不存在，未执行 ControlPlane 编译测试")

    web_root = ROOT / "Apps" / "WebConsole"
    node_modules = web_root / "node_modules"

    if command_available("pnpm") and node_modules.exists():
        steps.append(
            Step(
                "WebConsole Build（前端生产构建）",
                ("pnpm", "build"),
                cwd=web_root,
            )
        )
    elif require_frontend:
        if not command_available("pnpm"):
            steps.append(
                Step(
                    "pnpm Toolchain（前端包管理工具）",
                    ("pnpm", "--version"),
                    required=True,
                )
            )
        elif not node_modules.exists():
            print(
                "[FAIL] WebConsole/node_modules 不存在；"
                "质量门禁不会自动联网安装依赖"
            )
            steps.append(
                Step(
                    "WebConsole Dependencies（前端依赖）",
                    (
                        sys.executable,
                        "-c",
                        "import sys; sys.exit(1)",
                    ),
                    required=True,
                )
            )
    else:
        print(
            "[SKIP] 前端依赖未准备或 pnpm 不存在，"
            "未执行 WebConsole Build"
        )

    return steps


def main(argv: Sequence[str] | None = None) -> int:
    """执行项目统一质量门禁；任何必选步骤失败都立即返回非零退出码。"""

    parser = argparse.ArgumentParser(
        description="VisionAIPlatformWorkspace 统一生产质量门禁"
    )
    parser.add_argument(
        "--mode",
        choices=("quick", "full"),
        default="quick",
        help="quick=快速检查，full=严格构建 + 发布构建",
    )
    parser.add_argument(
        "--require-go",
        action="store_true",
        help="缺少 Go 工具链时也视为失败",
    )
    parser.add_argument(
        "--require-frontend",
        action="store_true",
        help="缺少 pnpm 或前端依赖时也视为失败",
    )

    args = parser.parse_args(argv)

    steps = build_steps(args.mode)
    steps.extend(
        optional_platform_steps(
            require_go=args.require_go,
            require_frontend=args.require_frontend,
        )
    )

    for step in steps:
        if not run_step(step):
            print("\nQUALITY GATE FAILED")
            return 1

    print("\nQUALITY GATE PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
