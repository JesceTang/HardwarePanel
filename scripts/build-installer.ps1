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
# Ninja 生成器需要 cl.exe/ninja/cmake 在 PATH。CI 已由 ilammy/msvc-dev-cmd 注入（此时 cl 已在
# PATH，直接跳过）；本地/独立运行时用 vswhere 可移植地定位 VS 安装，回退到已知候选路径，
# 避免写死开发机目录（本机 VS 在 E:\VSBuildTools，CI runner 在 C:\Program Files\...）。
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vcvars = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * `
                  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                  -property installationPath | Select-Object -First 1
        if ($vsPath) {
            $candidate = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path $candidate) { $vcvars = $candidate }
        }
    }
    if (-not $vcvars) {
        foreach ($p in @(
            'E:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat',
            'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
            'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
            'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
            'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
        )) {
            if (Test-Path $p) { $vcvars = $p; break }
        }
    }
    if (-not $vcvars) { throw '找不到 vcvars64.bat（需要 VS2022 MSVC 工具链；若已在 PATH 提供 cl.exe 则不会走到这里）' }
    cmd /c "`"$vcvars`" && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}
# VCPKG_ROOT：CI 由 workflow 注入（lukka/run-vcpkg 导出），本地开发回退到 E:\vcpkg。
# win-x64-ci preset 的 CMAKE_TOOLCHAIN_FILE 依赖该变量，缺失会导致 configure 失败。
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = 'E:\vcpkg' }
if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
    throw "VCPKG_ROOT 无效（缺 scripts\buildsystems\vcpkg.cmake）：$env:VCPKG_ROOT"
}

# ---- 定位 cmake / Qt / ISCC ----
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vsCMake = 'E:\VSBuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $vsCMake) { $cmake = $vsCMake } else { throw '找不到 cmake' }
} else { $cmake = $cmake.Source }

$qtDir = $env:Qt6_DIR
if (-not $qtDir) {
    # 本地回退：aqt 安装布局 E:\Qt\6.x\msvc2022_64\lib\cmake\Qt6
    $qtDir = Get-ChildItem 'E:\Qt\6.*\msvc2022_64\lib\cmake\Qt6' -ErrorAction SilentlyContinue |
             Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $qtDir) { throw '找不到 Qt6（设置 Qt6_DIR 或安装到 E:\Qt）' }
# Qt6_DIR 形如 ...\msvc2022_64\lib\cmake\Qt6，向上三级到套件根后取 bin
$qtBin = Join-Path (Split-Path (Split-Path (Split-Path $qtDir))) 'bin'
$windeployqt = Join-Path $qtBin 'windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    # 回退：CI 已将 qt_bin 加入 PATH
    $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if (-not $cmd) { throw "找不到 windeployqt.exe（推导路径 $windeployqt）" }
    $windeployqt = $cmd.Source
}

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

# vcpkg applocal 运行库：服务/客户端 exe 同目录下的依赖 DLL（grpc/protobuf/abseil/
# spdlog/openssl/re2/cares/zlib 等）必须一并 staging；否则安装到 Program Files 后
# 服务因缺 DLL 无法加载而启动失败（sc start 后立即 Stopped）。只拷 *.dll，
# 排除同目录的 .ilk/.pdb（数百 MB 调试产物）。
Copy-Item (Join-Path $svcExe.DirectoryName '*.dll') $staging -Force
Copy-Item (Join-Path $clientExe.DirectoryName '*.dll') $staging -Force

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
