; HWPanel Inno Setup 6 安装包脚本
; 前置：scripts\build-installer.ps1 已把 windeployqt 产物与二进制放入 staging 目录
; 手动编译：iscc /DBaseDir=<staging> installer\hwpanel.iss

#ifndef BaseDir
  #define BaseDir "..\out\installer"
#endif
#define MyAppName "HWPanel"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "HWPanel"
#define MyAppExeName "hwpanel-client.exe"
#define MySvcExeName "hwpanel-service.exe"
#define MySvcName "HWPanelService"

[Setup]
AppId={{7D3E5A10-9C4B-4E2F-8A6D-1B0F2C3D4E5F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\out\packages
OutputBaseFilename=HWPanel-Setup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=force

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; 客户端（windeployqt 已展开 Qt 运行库与 QML 插件）与服务端及依赖 dll，整树打包
Source: "{#BaseDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\HWPanel"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,HWPanel}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\HWPanel"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; 注册事件日志来源 HWPanel（M3 Event Log 验收）。
; EventLogSink 调 RegisterEventSourceW(nullptr, "HWPanel")；来源未注册时 Windows 降级到
; EventCreateGeneric，事件能写入但 Message 为空、且带 ProviderName 的查询会拒绝访问。
; 无自定义消息资源文件，故复用系统通用提供器使消息可渲染。
Root: HKLM64; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\HWPanel"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\HWPanel"; ValueType: expandsz; ValueName: "EventMessageFile"; ValueData: "{sys}\EventCreateGeneric.exe"; Flags: uninsdeletevalue
Root: HKLM64; Subkey: "SYSTEM\CurrentControlSet\Services\EventLog\Application\HWPanel"; ValueType: dword; ValueName: "TypesSupported"; ValueData: "7"; Flags: uninsdeletevalue

[Run]
; 注册服务：SYSTEM 账户、开机自启、崩溃自动重启（M3/M6 验收项）
Filename: "sc.exe"; Parameters: "create {#MySvcName} binPath= ""{app}\{#MySvcExeName}"" start= auto DisplayName= ""HWPanel Hardware Monitor Service"""; Flags: runhidden
Filename: "sc.exe"; Parameters: "description {#MySvcName} ""HWPanel hardware telemetry and configuration service (gRPC over named pipe)"""; Flags: runhidden
Filename: "sc.exe"; Parameters: "failure {#MySvcName} reset= 86400 actions= restart/10000/restart/10000/restart/10000"; Flags: runhidden
Filename: "sc.exe"; Parameters: "failureflag {#MySvcName} 1"; Flags: runhidden
Filename: "sc.exe"; Parameters: "start {#MySvcName}"; Flags: runhidden
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,HWPanel}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; 卸载清理：停止并删除服务（M6 验收：卸载→重装循环无残留）
Filename: "sc.exe"; Parameters: "stop {#MySvcName}"; Flags: runhidden; RunOnceId: "StopSvc"
Filename: "sc.exe"; Parameters: "delete {#MySvcName}"; Flags: runhidden; RunOnceId: "DelSvc"

[UninstallDelete]
; 服务运行期在 ProgramData 生成的日志/配置/缓存
Type: filesandordirs; Name: "{commonappdata}\HWPanel"
Type: dirifempty; Name: "{app}"

[Code]
// 安装前若旧服务仍在运行，先停止，避免文件占用
procedure StopExistingService();
var
  ResultCode: Integer;
begin
  Exec('sc.exe', 'stop {#MySvcName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(1500);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    StopExistingService();
end;
