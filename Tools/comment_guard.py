from __future__ import annotations

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]

# 仅检查真正的源码扩展名；JSON / YAML 不支持注释或并非所有文件都适合注释，因此不纳入强制规则。
SOURCE_SUFFIXES = {
    ".hpp",
    ".h",
    ".cpp",
    ".cc",
    ".cxx",
    ".go",
    ".py",
    ".ts",
    ".vue",
}

# 生成目录和第三方源码不属于本项目人工维护代码，避免误把自动生成/外部代码纳入注释规范。
EXCLUDED_PARTS = {
    ".artifacts",
    "node_modules",
    "ThirdParty",
}

CHINESE_RE = re.compile(r"[\u4e00-\u9fff]")


def has_chinese_comment(path: Path, text: str) -> bool:
    """判断源码中是否至少存在一处中文注释，作为人工审阅可读性的最低门槛。"""

    suffix = path.suffix.lower()

    if suffix in {".hpp", ".h", ".cpp", ".cc", ".cxx", ".go", ".ts"}:
        patterns = (
            re.compile(r"//[^\n]*[\u4e00-\u9fff]"),
            re.compile(r"/\*.*?[\u4e00-\u9fff].*?\*/", re.S),
        )
    elif suffix == ".py":
        patterns = (
            re.compile(r"#[^\n]*[\u4e00-\u9fff]"),
            re.compile(r'"""[\s\S]*?[\u4e00-\u9fff][\s\S]*?"""'),
        )
    elif suffix == ".vue":
        patterns = (
            re.compile(r"<!--.*?[\u4e00-\u9fff].*?-->", re.S),
            re.compile(r"/\*.*?[\u4e00-\u9fff].*?\*/", re.S),
            re.compile(r"//[^\n]*[\u4e00-\u9fff]"),
        )
    else:
        return True

    return any(pattern.search(text) for pattern in patterns)


def iter_source_files() -> list[Path]:
    """收集项目自有源码，排除构建制品、依赖目录和第三方代码。"""

    result: list[Path] = []
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue

        relative = path.relative_to(ROOT)
        if any(part in EXCLUDED_PARTS for part in relative.parts):
            continue

        result.append(path)

    return sorted(result)


def main() -> int:
    """执行中文注释门禁；缺少中文注释时返回非零退出码以阻断 CI（持续集成）。"""

    failures: list[str] = []

    for path in iter_source_files():
        text = path.read_text(encoding="utf-8")

        if not CHINESE_RE.search(text):
            failures.append(
                f"{path.relative_to(ROOT).as_posix()}: 源码中完全没有中文说明"
            )
            continue

        if not has_chinese_comment(path, text):
            failures.append(
                f"{path.relative_to(ROOT).as_posix()}: 存在中文文本，但没有中文注释"
            )

    if failures:
        print("Chinese Comment Guard FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print(f"Chinese Comment Guard PASS: {len(iter_source_files())} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
