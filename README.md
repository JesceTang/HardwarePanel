# HWPanel — Windows 硬件监控与配置中心

类 AMD Adrenalin / Armoury Crate 的简化实现：**Qt/QML 用户态客户端** 通过 **gRPC over Named Pipe** 与 **SYSTEM 权限 Windows 服务** 通信，服务侧完成硬件遥测采集（PDH / WMI / NVML）、配置档持久化与事务式应用（快照 → apply → 校验 → 回滚），客户端提供实时曲线、配置面板、配置档切换与通知中心。

## 架构

```
┌────────────────────────────┐         ┌──────────────────────────────────────┐
│  hwpanel-client (用户态)    │         │  hwpanel-service (LocalSystem, SCM)   │
│  Qt 6.8 / QML              │  gRPC   │                                      │
│                            │ protobuf│  ┌────────────┐   ┌───────────────┐  │
│  LineChart (自绘 QSGNode)   │◄───────►│  │ Telemetry  │   │ Command       │  │
│  TelemetryStore            │ np:\\.\ │  │ Service    │   │ Service       │  │
│  ProfileModel              │ pipe\   │  │ (服务端流)  │   │ (一元+事件流)  │  │
│  NotificationModel         │ hwpanel │  └─────┬──────┘   └──────┬────────┘  │
│  ConnectionStatus          │         │        │                 │           │
│  ClientWorker (gRPC 线程)   │  回退:   │  ┌─────▼──────┐   ┌──────▼────────┐  │
│   └ ChannelManager         │ 127.0.0.1:50051 │ TelemetryHub│  ProfileManager│ │
│     (看门狗+指数退避重连)    │         │  │ ring buffer │  │ 事务/回滚/热更新│  │
└────────────────────────────┘         │  └─────▲──────┘   └──────▲────────┘  │
                                       │        │                 │           │
                                       │  ┌─────┴─────────────────┴────────┐  │
                                       │  │ Collector Pipeline (1 Hz)      │  │
                                       │  │  PDH │ WMI │ NVML(动态加载)     │  │
                                       │  ├────────────────────────────────┤  │
                                       │  │ Executors: power.plan(powrprof)│  │
                                       │  │  display.brightness(dxva2)     │  │
                                       │  └────────────────────────────────┘  │
                                       │  观测: spdlog + EventLog + ETW        │
                                       └──────────────────────────────────────┘
```

### 仓库结构

| 目录 | 内容 |
|---|---|
| `proto/hwpanel/v1/` | telemetry.proto（服务端流）、command.proto（一元 + 事件流） |
| `libs/core/` | 遥测类型、ring buffer、TelemetryHub、日志、路径、endpoint、ChannelManager、EventBus |
| `libs/config/` | ProfileManager（事务应用/回滚/目录热更新监视） |
| `plugins/collectors/` | ICollector 接口 + PDH / WMI / NVML 插件 + 流水线 |
| `plugins/executors/` | IExecutor 接口 + 电源计划 / 显示器亮度插件 |
| `apps/hwpanel-service/` | SCM 集成、gRPC 服务实现、EventLog/ETW sink |
| `apps/hwpanel-client/` | Qt/QML 客户端（backend C++ 模型 + 自绘曲线） |
| `tools/hwpanel-smoke/` | 冒烟测试工具（订阅遥测流 N 秒并断言） |
| `tests/` | gtest 单测（协议往返、ring buffer、hub、流水线、配置事务、重连状态机等） |
| `installer/` | Inno Setup 6 脚本（服务注册/自启/崩溃恢复/卸载清理） |
| `scripts/` | 环境部署、服务安装、安装包构建脚本 |
| `.github/workflows/ci.yml` | CI：configure → build → ctest → 安装包 artifact |

## IPC 选型理由

**首选 gRPC over Named Pipe（`np:\\.\pipe\hwpanel`）**：

- 纯本地 IPC，无网络栈暴露面；管道不存在"端口被扫描/占用"问题。
- 安全由 **管道 ACL** 承载：服务以 LocalSystem 创建管道，安全描述符只授权 `SYSTEM`、`Administrators` 与交互用户（Authenticated Users 受限令牌），非授权进程无法连接——这是选 named pipe 而非 TCP 的核心原因。
- gRPC C++ 自 v1.4x 起原生支持 `np:` URI，protobuf 契约、服务端流、deadline 语义全部复用。

**回退 `127.0.0.1:50051`（localhost TCP）**：当 gRPC 版本对 `np:` 支持不足或管道创建失败时，服务端自动回退并记录告警（见 `service_host.cpp` 的 `BuildServer`）。回退模式下仅监听回环地址；README 记录该决策，客户端通过环境变量 `HWPANEL_ENDPOINT` 显式覆盖。

## 权限模型

| 组件 | 账户 | 权限来源 |
|---|---|---|
| hwpanel-service | `LocalSystem` | SCM 注册（`sc create ... start= auto`）；可读所有 PDH/WMI 计数器、调 powrprof 电源计划、写 `%ProgramData%\HWPanel` |
| named pipe ACL | — | SYSTEM + Administrators + 交互用户可读写；其余拒绝 |
| hwpanel-client | 登录用户 | 无特权；一切系统级操作经 gRPC 委托给服务 |
| 崩溃恢复 | SCM | `sc failure HWPanelService reset=86400 actions=restart/60000×3` + `failureflag 1`；客户端指数退避重连（1→15 s）+ ChannelManager 看门狗，双保险 |
| 配置写入 | 服务 | `%ProgramData%\HWPanel\profiles.json`，仅 SYSTEM/Administrators 可写（ACL 由服务创建时设置） |

厂商驱动级配置（风扇曲线/超频）需要内核驱动，超出本项目范围——executor 插件接口已预留，仅实现 OS 级可调项。

## 构建

前置：VS 2022 Build Tools（MSVC v143 + Win11 SDK + CMake/Ninja）、vcpkg、Qt 6.8 LTS（MSVC 2022 x64）。

```powershell
# 环境一键部署（E 盘）
scripts\setup-env.ps1
scripts\setup-vcpkg.ps1        # 或网络受限环境下 setup-vcpkg-mirror.ps1

# 构建 + 测试（本地 preset 自动发现 E:/Qt 与 E:/vcpkg）
cmake --preset win-x64
cmake --build --preset win-x64 --parallel
ctest --preset win-x64

# 冒烟：先起服务（控制台模式），再订阅遥测流 10 秒
out\build\win-x64\apps\hwpanel-service\hwpanel-service.exe --console
out\build\win-x64\tools\hwpanel-smoke\hwpanel-smoke.exe --seconds 10

# 安装包（windeployqt + Inno Setup 6 → out\packages\HWPanel-Setup-*.exe）
scripts\build-installer.ps1
```

手动安装/卸载服务（管理员）：

```powershell
scripts\install-service.ps1                # 注册 + 自启 + 崩溃恢复 + 启动
scripts\install-service.ps1 -Uninstall     # 停止 + 删除
```

## 可观测性（EventLog / ETW）

服务端日志三路输出：spdlog（控制台 + `%ProgramData%\HWPanel\logs\` 滚动文件）、Windows 事件日志（源 `HWPanel`，warn 及以上）、ETW TraceLogging provider **`HWPanel.Service`**（GUID `{8F3A2B1C-4D5E-4F6A-9B8C-7D6E5F4A3B2C}`，每条日志一个事件）。

```powershell
# 事件日志
Get-WinEvent -LogName Application -MaxEvents 50 | Where-Object ProviderName -eq 'HWPanel'

# ETW 实时捕获（管理员，Windows SDK tracelog / PerfView / wpr）
logman start hwpanel-etw -p "{8F3A2B1C-4D5E-4F6A-9B8C-7D6E5F4A3B2C}" 0xffffffffffffffff 0xff -o C:\Temp\hwpanel.etl -ets
# ... 复现问题 ...
logman stop hwpanel-etw -ets
tracerpt C:\Temp\hwpanel.etl -o C:\Temp\hwpanel-etw.xml
# 或: wpr -start GeneralProfile -PtcGuid 0x8f3a2b1c,0xffffffffffffffff,0xff "HWPanel.Service" -filemode
```

## 里程碑与验收

| 里程碑 | 内容 | 验收 |
|---|---|---|
| M0 | 工具链 + 仓库骨架 + CMakePresets + vcpkg.json + CI | 双进程 hello-world CI 绿 |
| M1 | proto + named pipe/ACL + ChannelManager 重连 + 单测 | named pipe 1 Hz 流式 10 min 零断连 |
| M2 | PDH/WMI/NVML 采集 + 多线程流水线 + fake collector 单测 | 全指标 1 Hz，服务 CPU <2% |
| M3 | SCM 服务化 + EventLog/ETW + sc failure 崩溃恢复 | 重启自活；taskkill 后 ≤30 s 自恢复 |
| M4 | profile JSON + 事务 apply/rollback + executor 热更新 | executor 失败自动回滚并推送通知 |
| M5 | QML 客户端：自绘曲线/面板/切档/通知/断线状态条 | UI 切档 → 服务 apply → 曲线/通知联动闭环 |
| M6 | Inno 安装包 + README + CI artifact | 本地卸载→重装循环无残留 |

## 关键设计决策

1. **实时曲线自绘**：`LineChart`（QQuickItem + QSGGeometryNode DrawLineStrip），不引入 Qt Charts——避免许可约束并获得完全的场景图级控制。
2. **PDH 本地化免疫**：计数器一律经英文名 → counter ID → 本地名解析（`PdhLookupPerfIndexByEnglishNameW`），中文系统不受影响；通配实例（多 GPU/多磁盘）用 `PdhExpandWildCardPathW` 求和。
3. **NVML 动态加载**：`LoadLibrary("nvml.dll")` 从驱动目录解析，不捆绑 dll（再分发限制）；无 NVIDIA 独显时优雅降级为 PDH GPU Engine 计数器。
4. **遥测节奏**：1 Hz 采样与推送；客户端曲线窗口 60 s；服务端 ring buffer 保留 5 min；订阅者队列 64 满时 drop-oldest（慢消费者不拖垮流水线）。
5. **配置事务**：`SwitchTo` = 定位 executor → 快照当前值 → 逐项 apply → 任一失败逆序回滚并上报 `rolled_back_keys` → 成功后持久化 active id 并推送事件。`%ProgramData%\HWPanel` 目录经 `ReadDirectoryChangesW` 监视，外部改档即时热加载。
