; WinCamHTTP Inno Setup script
#define DefaultPlatform "x64"
#ifndef Platform
  #define Platform DefaultPlatform
#endif

#define DefaultConfiguration "Release"
#ifndef Configuration
  #define Configuration DefaultConfiguration
#endif

#define BinRoot "..\\"+ Platform + "\\" + Configuration
#ifnexist BinRoot + "\\WinCamHTTP.exe"
  #pragma error "Build output not found at " + BinRoot + ". Run msbuild WinCamHTTP.sln first or override /DConfiguration and /DPlatform."
#endif

#define AppVersion GetVersionNumbersString(BinRoot + "\\WinCamHTTP.exe")

[Setup]
AppId={{0B35C96B-5D09-4BD2-9EFF-087AE6FD6395}}
AppName=WinCamHTTP
AppVersion={#AppVersion}
AppPublisher=WinCamHTTP
AppPublisherURL=https://github.com/kbralten/WinCamHTTP
DefaultDirName={autopf64}\WinCamHTTP
DisableDirPage=yes
DefaultGroupName=WinCamHTTP
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=Output
OutputBaseFilename=WinCamHTTPSetup
Compression=lzma
SolidCompression=yes
SetupIconFile=..\WinCamHTTP\WinCamHTTP.ico
UninstallDisplayIcon={app}\WinCamHTTP.exe
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BinRoot}\\WinCamHTTP.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit
Source: "{#BinRoot}\\WinCamHTTPSetup.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit
Source: "{#BinRoot}\\WinCamHTTPSource.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 64bit
Source: "{#BinRoot}\\WinCamHTTP.pdb"; DestDir: "{app}"; Flags: ignoreversion 64bit skipifsourcedoesntexist
Source: "{#BinRoot}\\WinCamHTTPSetup.pdb"; DestDir: "{app}"; Flags: ignoreversion 64bit skipifsourcedoesntexist
Source: "{#BinRoot}\\WinCamHTTPSource.pdb"; DestDir: "{app}"; Flags: ignoreversion 64bit skipifsourcedoesntexist
Source: "..\\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\\WinCamHTTP"; Filename: "{app}\\WinCamHTTP.exe"; WorkingDir: "{app}"
Name: "{group}\\WinCamHTTP Setup"; Filename: "{app}\\WinCamHTTPSetup.exe"; WorkingDir: "{app}"; Comment: "Configure MJPEG source and camera settings"
Name: "{group}\\Uninstall WinCamHTTP"; Filename: "{uninstallexe}"
Name: "{autodesktop}\\WinCamHTTP"; Filename: "{app}\\WinCamHTTP.exe"; Tasks: desktopicon

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: autostart; Description: "Start WinCamHTTP when Windows starts"; GroupDescription: "Automatic startup:"; Flags: unchecked

[Registry]
Root: HKLM; Subkey: "SOFTWARE\\WinCamHTTP"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\\WinCamHTTP"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"; ValueType: string; ValueName: "WinCamHTTP"; ValueData: "{app}\\WinCamHTTP.exe"; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{sys}\\regsvr32.exe"; Parameters: "/s ""{app}\\WinCamHTTPSource.dll"""; Flags: runhidden waituntilterminated
Filename: "{app}\\WinCamHTTPSetup.exe"; Description: "Run WinCamHTTP Setup after installing"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\\regsvr32.exe"; Parameters: "/s /u ""{app}\\WinCamHTTPSource.dll"""; Flags: runhidden waituntilterminated
