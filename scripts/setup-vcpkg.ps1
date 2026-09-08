# vcpkg 部署（带重试，应对 github 间歇性连接失败）
$ErrorActionPreference = 'Continue'
$log = 'e:\CCworkingspace\HWPanel\.dev\vcpkg.log'
New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null

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

"=== vcpkg setup start $(Get-Date -Format s)" | Add-Content $log
if (-not (Test-Path E:\vcpkg\vcpkg.exe)) {
    if (-not (Test-Path E:\vcpkg\.git)) {
        Invoke-WithRetry { git clone --depth 1 https://github.com/microsoft/vcpkg.git E:\vcpkg *>> $log } 'clone'
    }
    Invoke-WithRetry { cmd /c "E:\vcpkg\bootstrap-vcpkg.bat -disableMetrics" *>> $log } 'bootstrap'
}
"=== vcpkg.exe present: $(Test-Path E:\vcpkg\vcpkg.exe)" | Add-Content $log
"=== vcpkg setup done $(Get-Date -Format s)" | Add-Content $log
