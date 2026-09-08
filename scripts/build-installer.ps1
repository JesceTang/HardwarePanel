# HWPanel 安装包构建：release 构建 → windeployqt 部署 → Inno Setup 编译
# 用法: scripts\build-installer.ps1 [-Preset win-x64]
param(
    [string]$Preset = 'win-x64'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repo "out\build\$Preset"
$staging = Join-Path $repo 'out\installer'

# ---- MSVC 工具链环境（vcvars），保证裸 shell 下 configure/build 可用 ----
$vcvars = 'E:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat'
if (Test-Path $vcvars) {
    cmd /c "`"$vcvars`" && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}
$env:VCPKG_ROOT = 'E:\vcpkg'

# ---- 定位 cmake / Qt / ISCC ----
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vsCMake = 'E:\VSBuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $vsCMake) { $cmake = $vsCMake } else { throw '找不到 cmake' }
} else { $cmake = $cmake.Source }

$qtDir = $env:Qt6_DIR
if (-not $qtDir) {
    $qtDir = Get-ChildItem 'E:\Qt\6.*\msvc2022_64\lib\cmake\Qt6' -ErrorAction SilentlyContinue |
             Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $qtDir) { throw '找不到 Qt6（设置 Qt6_DIR 或安装到 E:\Qt）' }
$qtBin = Join-Path (Split-Path (Split-Path (Split-Path $qtDir))) 'bin'   # ...\msvc2022_64\bin
$windeployqt = Join-Path $qtBin 'windeployqt.exe'

$iscc = Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $iscc) {
    foreach ($p in @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
                     "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                     "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $p) { $iscc = $p; break }
    }
}
if (-not $iscc) { throw '找不到 Inno Setup 6 (ISCC.exe)' }

# ---- 构建 ----
& $cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { throw "configure 失败" }
& $cmake --build --preset $Preset --parallel
if ($LASTEXITCODE -ne 0) { throw "build 失败" }

# ---- 组装 staging ----
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

$svcExe = Get-ChildItem -Recurse (Join-Path $buildDir 'apps\hwpanel-service') -Filter hwpanel-service.exe | Select-Object -First 1
$clientExe = Get-ChildItem -Recurse (Join-Path $buildDir 'apps\hwpanel-client') -Filter hwpanel-client.exe | Select-Object -First 1
if (-not $svcExe -or -not $clientExe) { throw '未找到构建产物 hwpanel-service.exe / hwpanel-client.exe' }

Copy-Item $svcExe.FullName $staging
Copy-Item $clientExe.FullName $staging

# Qt 运行库 + QML 插件部署（客户端为 GUI 程序）
& $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    --qmldir (Join-Path $repo 'apps\hwpanel-client\qml') `
    (Join-Path $staging 'hwpanel-client.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt 失败' }

# ---- Inno 编译 ----
& $iscc "/DBaseDir=$staging" (Join-Path $repo 'installer\hwpanel.iss')
if ($LASTEXITCODE -ne 0) { throw 'ISCC 编译失败' }

Write-Host "安装包输出: $(Join-Path $repo 'out\packages')"
Get-ChildItem (Join-Path $repo 'out\packages') | Select-Object Name, Length
