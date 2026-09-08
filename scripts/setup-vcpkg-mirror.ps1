# 通过镜像部署 vcpkg（github 主站不可达时的回退路径）：
# 1) ghproxy 下载 vcpkg 源码 zip（master）→ $VcpkgRoot
# 2) ghproxy 下载 release 预编译 vcpkg.exe（免 bootstrap 编译）
# 用法: scripts\setup-vcpkg-mirror.ps1 [-VcpkgRoot E:\vcpkg]
param(
    [string]$VcpkgRoot = $(if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'E:\vcpkg' })
)

$ErrorActionPreference = 'Stop'
# 日志与临时目录一律基于仓库根推导，不绑定开发机绝对路径
$repoRoot = Split-Path -Parent $PSScriptRoot
$tmp = Join-Path $repoRoot '.dev'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$log = Join-Path $tmp 'vcpkg.log'
function Log([string]$m) { $m | Add-Content $log; Write-Host $m }

$bootstrap = Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat'
$exe = Join-Path $VcpkgRoot 'vcpkg.exe'

Log "=== mirror setup start $(Get-Date -Format s) root=$VcpkgRoot"

$mirrors = @('https://ghproxy.net/', 'https://gh-proxy.com/')

function Download-WithMirrors([string]$relativeUrl, [string]$dest) {
    foreach ($prefix in $mirrors) {
        $url = $prefix + $relativeUrl
        try {
            Log "--- downloading $url"
            Invoke-WebRequest -Uri $url -OutFile $dest -TimeoutSec 600 -UseBasicParsing
            Log "--- OK -> $dest ($((Get-Item $dest).Length) bytes)"
            return $true
        } catch {
            Log "--- FAIL: $($_.Exception.Message)"
        }
    }
    # 最后尝试直连
    try {
        Log "--- downloading direct $relativeUrl"
        Invoke-WebRequest -Uri $relativeUrl -OutFile $dest -TimeoutSec 600 -UseBasicParsing
        Log "--- OK direct -> $dest"
        return $true
    } catch {
        Log "--- FAIL direct: $($_.Exception.Message)"
        return $false
    }
}

if (-not (Test-Path $bootstrap)) {
    # 只在确认是 vcpkg 源码树（或空目录）时才清理，避免误删无关内容
    if ((Test-Path $VcpkgRoot) -and -not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.json'))) {
        Remove-Item -Recurse -Force $VcpkgRoot
    }
    $zip = Join-Path $tmp 'vcpkg-src.zip'
    if (-not (Download-WithMirrors 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip' $zip)) {
        Log '=== GAVE UP: source zip'; exit 1
    }
    Expand-Archive -Path $zip -DestinationPath $tmp -Force
    Move-Item (Join-Path $tmp 'vcpkg-master') $VcpkgRoot
    Remove-Item $zip -Force
    Log "=== source tree in place: $(Test-Path $bootstrap)"
}

if (-not (Test-Path $exe)) {
    $exeZip = Join-Path $tmp 'vcpkg-exe.zip'
    if (Download-WithMirrors 'https://github.com/microsoft/vcpkg/releases/latest/download/vcpkg.exe' $exeZip) {
        # releases/latest/download/vcpkg.exe 实际是裸 exe；若下载为 zip 则解压
        $bytes = [System.IO.File]::ReadAllBytes($exeZip)[0..1]
        if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) {
            Move-Item $exeZip $exe -Force
        } else {
            Expand-Archive -Path $exeZip -DestinationPath $tmp -Force
            Move-Item (Join-Path $tmp 'vcpkg.exe') $exe -Force
            Remove-Item $exeZip -Force
        }
    }
}

if (-not (Test-Path $exe)) {
    Log '=== prebuilt exe unavailable, trying bootstrap (likely fails without github)'
    Push-Location $VcpkgRoot
    cmd /c "bootstrap-vcpkg.bat -disableMetrics" *>> $log
    Pop-Location
}

Log "=== vcpkg.exe present: $(Test-Path $exe)"
if (Test-Path $exe) { & $exe version *>> $log }
Log "=== mirror setup done $(Get-Date -Format s)"
