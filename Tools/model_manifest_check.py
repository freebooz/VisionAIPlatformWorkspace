from pathlib import Path

REQUIRED_KEYS = {
    "model_id:",
    "version:",
    "capability:",
    "architecture:",
    "input_schema:",
    "output_schema:",
}

def main() -> int:
    """检查所有 Model Package（模型包）清单是否包含当前工程要求的最小顶层字段。"""
    root = Path(__file__).resolve().parents[1]
    manifests = list((root / "ModelPackages").rglob("manifest.yaml"))
    if not manifests:
        raise SystemExit("未找到任何 Model Package（模型包）清单")

    for manifest in manifests:
        text = manifest.read_text(encoding="utf-8")
        missing = [key for key in REQUIRED_KEYS if key not in text]
        if missing:
            raise SystemExit(f"{manifest}: 缺少字段 {missing}")

    print(f"Model Package manifest check PASS: {len(manifests)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
