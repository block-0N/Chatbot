; ============================================================
;  Chatbot v2.1 Inno Setup 安装脚本
;  用 Inno Setup Compiler 打开本文件 → Build → Compile
; ============================================================

#define MyAppName "C++ 离线聊天机器人"
#define MyAppVersion "2.0"
#define MyAppPublisher "Your Name"
#define MyAppExeName "functions.exe"

[Setup]
AppId={{B4A2E5F1-8C3D-4E6A-9B7F-1D2E3C4A5B6C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Chatbot
DefaultGroupName={#MyAppName}
OutputDir=Output
OutputBaseFilename=Chatbot_v2.1_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加图标:"; Flags: unchecked

[Files]
Source: "..\release\functions.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\release\dict.exe";      DestDir: "{app}"; Flags: ignoreversion
Source: "..\data\dict.dat";         DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "..\data\data.dat";         DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "..\data\config\*";         DestDir: "{app}\config"; Flags: recursesubdirs createallsubdirs onlyifdoesntexist
Source: "..\README.md";             DestDir: "{app}"; Flags: isreadme

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"
Name: "{group}\词库管理工具";         Filename: "{app}\dict.exe"
Name: "{group}\卸载 {#MyAppName}";    Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\config"
Type: files; Name: "{app}\*.tmp"