param(
    [ValidateSet("quick", "full")]
    [string]$Mode = "quick",

    [switch]$RequireGo,
    [switch]$RequireFrontend
)

# 统一入口：PowerShell（微软脚本环境）只负责参数转发，
# 实际质量门禁逻辑集中在 Tools/quality_gate.py，避免 Windows/Linux 两套规则漂移。
$Arguments = @(
    "Tools/quality_gate.py",
    "--mode",
    $Mode
)

if ($RequireGo) {
    $Arguments += "--require-go"
}

if ($RequireFrontend) {
    $Arguments += "--require-frontend"
}

python @Arguments
exit $LASTEXITCODE
