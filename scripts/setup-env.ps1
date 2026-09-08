# HWPanel 开发环境一键安装脚本（需管理员/UAC 同意）
# 用法: powershell -ExecutionPolicy Bypass -File scripts\setup-env.ps1
$ErrorActionPreference = 'Continue'
$dev = 'e:\CCworkingspace\HWPanel\.dev'
New-Item -ItemType Directory -Force -Path $dev | Out-Null
$log = Join-Path $dev 'setup.log'

function Step([string]$name, [scriptblock]$body) {
    "=== [$name] START $(Get-Date -Format s)" | Add-Content $log
    & $body *>> $log
    "=== [$name] EXIT $LASTEXITCODE $(Get-Date -Format s)" | Add-Content $log
}

Step 'vs-buildtools' {
    winget install --exact --id Microsoft.VisualStudio.2022.BuildTools --silent `
        --override '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --installPath E:\VSBuildTools' `
        --accept-package-agreements --accept-source-agreements
}

Step 'vcpkg' {
    git clone --depth 1 https://github.com/microsoft/vcpkg.git E:\vcpkg
    & E:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
}

Step 'inno-setup' {
    winget install --exact --id JRSoftware.InnoSetup --silent `
        --accept-package-agreements --accept-source-agreements
}

Step 'qt-aqt' {
    python -m pip install --user aqtinstall
    python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O E:\Qt
}

"=== ALL DONE $(Get-Date -Format s)" | Add-Content $log
