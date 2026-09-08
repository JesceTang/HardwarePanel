# 通过镜像部署 vcpkg（github 主站不可达）：
# 1) ghproxy 下载 vcpkg 源码 zip（master）→ E:\vcpkg
# 2) ghproxy 下载 release 预编译 vcpkg.exe（免 bootstrap 编译）
$ErrorActionPreference = 'Stop'
$log = 'e:\CCworkingspace\HWPanel\.dev\vcpkg.log'
function Log([string]$m) { $m | Add-Content $log; Write-Host $m }

Log "=== mirror setup start $(Get-Date -Format s)"

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

$tmp = 'e:\CCworkingspace\HWPanel\.dev'

if (-not (Test-Path E:\vcpkg\bootstrap-vcpkg.bat)) {
    if (Test-Path E:\vcpkg) { Remove-Item -Recurse -Force E:\vcpkg }
    $zip = Join-Path $tmp 'vcpkg-src.zip'
    if (-not (Download-WithMirrors 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip' $zip)) {
        Log '=== GAVE UP: source zip'; exit 1
    }
    Expand-Archive -Path $zip -DestinationPath $tmp -Force
    Move-Item (Join-Path $tmp 'vcpkg-master') 'E:\vcpkg'
    Remove-Item $zip -Force
    Log "=== source tree in place: $(Test-Path E:\vcpkg\bootstrap-vcpkg.bat)"
}

if (-not (Test-Path E:\vcpkg\vcpkg.exe)) {
    $exeZip = Join-Path $tmp 'vcpkg-exe.zip'
    if (Download-WithMirrors 'https://github.com/microsoft/vcpkg/releases/latest/download/vcpkg.exe' $exeZip) {
        # releases/latest/download/vcpkg.exe 实际是裸 exe；若下载为 zip 则解压
        $bytes = [System.IO.File]::ReadAllBytes($exeZip)[0..1]
        if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) {
            Move-Item $exeZip 'E:\vcpkg\vcpkg.exe' -Force
        } else {
            Expand-Archive -Path $exeZip -DestinationPath $tmp -Force
            Move-Item (Join-Path $tmp 'vcpkg.exe') 'E:\vcpkg\vcpkg.exe' -Force
            Remove-Item $exeZip -Force
        }
    }
}

if (-not (Test-Path E:\vcpkg\vcpkg.exe)) {
    Log '=== prebuilt exe unavailable, trying bootstrap (likely fails without github)'
    Push-Location E:\vcpkg
    cmd /c "bootstrap-vcpkg.bat -disableMetrics" *>> $log
    Pop-Location
}

Log "=== vcpkg.exe present: $(Test-Path E:\vcpkg\vcpkg.exe)"
if (Test-Path E:\vcpkg\vcpkg.exe) { & E:\vcpkg\vcpkg.exe version *>> $log }
Log "=== mirror setup done $(Get-Date -Format s)"
