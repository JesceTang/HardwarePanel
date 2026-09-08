# vcpkg 部署（带重试，应对 github 间歇性连接失败）
# 用法: scripts\setup-vcpkg.ps1 [-VcpkgRoot E:\vcpkg]
param(
    [string]$VcpkgRoot = $(if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'E:\vcpkg' })
)

$ErrorActionPreference = 'Continue'
# 日志与临时目录一律基于仓库根推导，不绑定开发机绝对路径
$repoRoot = Split-Path -Parent $PSScriptRoot
$devDir = Join-Path $repoRoot '.dev'
$log = Join-Path $devDir 'vcpkg.log'
New-Item -ItemType Directory -Force -Path $devDir | Out-Null

function Invoke-WithRetry([scriptblock]$body, [string]$what, [int]$tries = 8, [int]$delay = 5) {
    for ($i = 1; $i -le $tries; $i++) {
        & $body
        if ($LASTEXITCODE -eq 0) {
            "=== [$what] OK on attempt $i" | Add-Content $log
            return $true
        }
        "=== [$what] attempt $i failed (exit $LASTEXITCODE), retrying" | Add-Content $log
        Start-Sleep -Seconds $delay
    }
    "=== [$what] GAVE UP" | Add-Content $log
    return $false
}

"=== vcpkg setup start $(Get-Date -Format s) root=$VcpkgRoot" | Add-Content $log
$bootstrap = Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat'
$exe = Join-Path $VcpkgRoot 'vcpkg.exe'
if (-not (Test-Path $exe)) {
    if (-not (Test-Path (Join-Path $VcpkgRoot '.git'))) {
        Invoke-WithRetry { git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot *>> $log } 'clone'
    }
    Invoke-WithRetry { cmd /c "`"$bootstrap`" -disableMetrics" *>> $log } 'bootstrap'
}
"=== vcpkg.exe present: $(Test-Path $exe)" | Add-Content $log
"=== vcpkg setup done $(Get-Date -Format s)" | Add-Content $log
