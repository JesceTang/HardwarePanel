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
Start-Sleep -Seconds 2
$events = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; ProviderName = 'HWPanel' } `
            -MaxEvents 5 -ErrorAction SilentlyContinue
if (-not $events) { throw 'no HWPanel events in Application log' }
Write-Host "[smoke] event log source HWPanel present ($($events.Count) recent events)"

# ---- taskkill → SCM 崩溃恢复（sc failure restart/10000）应 ≤30 s 自恢复 ----
$proc = Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { throw 'hwpanel-service process not found' }
$oldPid = $proc.Id
taskkill /F /PID $oldPid | Out-Null
Write-Host "[smoke] killed pid $oldPid, awaiting SCM recovery (<=30s)"
$deadline = (Get-Date).AddSeconds(30)
$recovered = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if (Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue) { $recovered = $true; break }
}
if (-not $recovered) { throw 'service did not self-recover within 30 s' }
Write-Host "[smoke] self-recovered within 30 s"

# ---- 暂停 / 继续 ----
sc.exe pause $svc | Out-Null
Start-Sleep -Seconds 2
if ((Get-Service -Name $svc).Status -ne 'Paused') { throw 'pause did not reach Paused' }
sc.exe continue $svc | Out-Null
Start-Sleep -Seconds 2
if ((Get-Service -Name $svc).Status -ne 'Running') { throw 'continue did not reach Running' }
Write-Host "[smoke] pause/continue OK"

# ---- 可选：卸载并断言无残留 ----
if ($UninstallAtEnd) {
    & (Join-Path $PSScriptRoot 'install-service.ps1') -Uninstall
    if (Get-Service -Name $svc -ErrorAction SilentlyContinue) { throw 'service residue after uninstall' }
    if (Get-Process -Name hwpanel-service -ErrorAction SilentlyContinue) { throw 'process residue after uninstall' }
    Write-Host '[smoke] uninstall clean'
}

Write-Host '[smoke] M3 service smoke PASSED'
