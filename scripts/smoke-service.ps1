# HWPanel M3 服务冒烟（需管理员；CI windows-latest runner 默认具备）
# 覆盖计划 M3 验收：服务 RUNNING / 事件日志断言 / taskkill 后 ≤30 s 自恢复 / 暂停继续。
# 安装与卸载由调用方负责（CI 用安装包，本地用 install-service.ps1），本脚本只做运行时断言。
param(
    [switch]$UninstallAtEnd
)

$ErrorActionPreference = 'Stop'
$svc = 'HWPanelService'

# ---- 服务已安装且 RUNNING ----
Start-Sleep -Seconds 3
$state = (Get-Service -Name $svc -ErrorAction SilentlyContinue).Status
if ($state -ne 'Running') { throw "service not Running: $state" }
Write-Host "[smoke] service RUNNING"

# ---- 事件日志断言：EventLogSink 以来源 HWPanel 写入 Application 日志 ----
# 注：EventLogSink 用 RegisterEventSourceW(nullptr, "HWPanel")，来源未注册时 Windows 会降级到
# EventCreateGeneric 写入；此时带 ProviderName 的 FilterHashtable 查询会报拒绝访问（本机 Win11
# 家庭版已实测）。故改为无过滤器读取 + 客户端过滤，并给出重试窗口等待日志落盘。
$events = @()
for ($i = 0; $i -lt 6; $i++) {
    Start-Sleep -Seconds 2
    $recent = Get-WinEvent -FilterHashtable @{ LogName = 'Application' } -MaxEvents 300 `
              -ErrorAction SilentlyContinue
    $events = @($recent | Where-Object { $_.ProviderName -eq 'HWPanel' })
    if ($events.Count -gt 0) { break }
}
if ($events.Count -eq 0) { throw 'no HWPanel events in Application log' }
Write-Host "[smoke] event log source HWPanel present ($($events.Count) events)"

# ---- taskkill → SCM 崩溃恢复（sc failure restart/10000）应 ≤30 s 自恢复 ----
# 自恢复判据取“服务重新进入 Running”，而非仅进程存在：SCM 重启进程后仍需走完
# START_PENDING→RUNNING，此期间无法稳定暂停。以 Running 为准既贴合验收语义，也保证
# 随后的 pause 针对已完全启动的服务（避免与启动窗口竞态）。
$proc = Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { throw 'hwpanel-service process not found' }
$oldPid = $proc.Id
taskkill /F /PID $oldPid | Out-Null
Write-Host "[smoke] killed pid $oldPid, awaiting SCM recovery to Running (<=30s)"
$deadline = (Get-Date).AddSeconds(30)
$recovered = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Get-Service -Name $svc -ErrorAction SilentlyContinue).Status -eq 'Running') { $recovered = $true; break }
}
if (-not $recovered) { throw 'service did not self-recover to Running within 30 s' }
Write-Host "[smoke] self-recovered to Running within 30 s"

# ---- 暂停 / 继续（轮询等待状态迁移，替代固定 sleep）----
function Wait-SvcState([string]$target, [int]$timeoutSec) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $timeoutSec) {
        if ((Get-Service -Name $svc -ErrorAction SilentlyContinue).Status -eq $target) { return $true }
        Start-Sleep -Milliseconds 200
    }
    return $false
}
sc.exe pause $svc | Out-Null
if (-not (Wait-SvcState 'Paused' 10)) { throw 'pause did not reach Paused' }
sc.exe continue $svc | Out-Null
if (-not (Wait-SvcState 'Running' 10)) { throw 'continue did not reach Running' }
Write-Host "[smoke] pause/continue OK"

# ---- 可选：卸载并断言无残留 ----
if ($UninstallAtEnd) {
    & (Join-Path $PSScriptRoot 'install-service.ps1') -Uninstall
    # 轮询确认无残留：服务删除与进程退出可能有短暂滞后（install-service 已含等待+兜底）。
    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-Service -Name $svc -ErrorAction SilentlyContinue) -and
            -not (Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 300
    }
    if (Get-Service -Name $svc -ErrorAction SilentlyContinue) { throw 'service residue after uninstall' }
    if (Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue) { throw 'process residue after uninstall' }
    Write-Host '[smoke] uninstall clean'
}

Write-Host '[smoke] M3 service smoke PASSED'
