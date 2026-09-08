# HWPanel 服务注册脚本（需管理员权限）
# 用法: scripts\install-service.ps1 [-ExePath <hwpanel-service.exe 路径>] [-Uninstall]
param(
    [string]$ExePath = '',
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$ServiceName = 'HWPanelService'
$DisplayName = 'HWPanel Hardware Monitor Service'
$Description = 'HWPanel 硬件遥测采集与配置应用服务（gRPC over named pipe）'

# 校验管理员权限
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw '需要管理员权限运行（服务注册/删除）。'
}

if ($Uninstall) {
    $svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
    if ($svc) {
        if ($svc.Status -ne 'Stopped') { Stop-Service -Name $ServiceName -Force }
        sc.exe delete $ServiceName | Out-Null
        Write-Host "服务 $ServiceName 已删除。"
    } else {
        Write-Host "服务 $ServiceName 不存在。"
    }
    exit 0
}

if (-not $ExePath) {
    # 默认取安装目录或本地构建输出
    $candidates = @(
        (Join-Path $env:ProgramFiles 'HWPanel\hwpanel-service.exe'),
        'e:\CCworkingspace\HWPanel\out\build\win-x64\apps\hwpanel-service\hwpanel-service.exe'
    )
    $ExePath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $ExePath -or -not (Test-Path $ExePath)) {
    throw "找不到 hwpanel-service.exe，请用 -ExePath 指定。"
}
$ExePath = (Resolve-Path $ExePath).Path

# 已存在则先停掉并删除（幂等重装）
$existing = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($existing) {
    if ($existing.Status -ne 'Stopped') { Stop-Service -Name $ServiceName -Force }
    sc.exe delete $ServiceName | Out-Null
    Start-Sleep -Seconds 2
}

sc.exe create $ServiceName binPath= "`"$ExePath`"" start= auto DisplayName= "$DisplayName" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "sc create 失败 (exit $LASTEXITCODE)" }
sc.exe description $ServiceName "$Description" | Out-Null

# 崩溃恢复：10 s 内自动重启（计划 M3 验收 taskkill 后 ≤30 s 自恢复），24 小时窗口
sc.exe failure $ServiceName reset= 86400 actions= restart/10000/restart/10000/restart/10000 | Out-Null
sc.exe failureflag $ServiceName 1 | Out-Null   # 非零退出码也触发恢复

Start-Service -Name $ServiceName
Write-Host "服务 $ServiceName 已安装并启动（binPath=$ExePath）。"
sc.exe query $ServiceName
