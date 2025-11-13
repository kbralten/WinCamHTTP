# WinCamHTTP Installer (Inno Setup)

This directory now contains an [Inno Setup 6](https://jrsoftware.org/isinfo.php) script that packages the WinCamHTTP binaries into a distributable EXE installer with COM registration and Start Menu integration.

## Prerequisites

- **Visual Studio 2022** (or Build Tools) with the Desktop C++ workload
- **MSBuild** available on `PATH` (Developer PowerShell/Prompt)
- **Inno Setup 6** installed. Any of the following options work:
  - Official installer (default path: `C:\Program Files (x86)\Inno Setup 6\ISCC.exe`)
  - Microsoft Store install (`%LOCALAPPDATA%\Programs\Inno Setup 6`)
  - Chocolatey: `choco install innosetup --no-progress -y`

## Building the Installer

### Automated build (recommended)

```powershell
# Build binaries + compile installer (auto-detects ISCC.exe)
./build-installer.ps1 -Configuration Release -Platform x64
```

The script will:
1. Build `WinCamHTTP.sln`
2. Locate `ISCC.exe`
3. Compile `Installer/WinCamHTTP.iss`
4. Drop `Installer/Output/WinCamHTTPSetup.exe` and copy it to `x64/Release/WinCamHTTPSetup.exe`

For CI/non-admin scenarios, use the lighter script:

```powershell
# Requires msbuild + iscc already on PATH
./build-installer-nuget.ps1 -Configuration Release -Platform x64
```

### Manual build via Inno Setup

1. Build the native binaries (Release/x64):
   ```powershell
   msbuild WinCamHTTP.sln /p:Configuration=Release /p:Platform=x64 /m
   ```
2. Compile the installer:
   ```powershell
   & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" \
     "/DConfiguration=Release" \
     "/DPlatform=x64" \
     "Installer\WinCamHTTP.iss"
   ```
3. The installer is emitted to `Installer/Output/WinCamHTTPSetup.exe`.

## Installer Features

- Installs `WinCamHTTP.exe`, `WinCamHTTPSetup.exe`, and `WinCamHTTPSource.dll` to `C:\Program Files\WinCamHTTP\`
- Runs `regsvr32 /s` during install (and `/u` on uninstall) to guarantee COM registration
- Creates Start Menu shortcuts for both apps and the uninstaller
- Adds optional desktop and "Start WinCamHTTP with Windows" tasks
- Writes `HKLM\SOFTWARE\WinCamHTTP` values for install path and version
- Supports silent install/uninstall switches:
  - `WinCamHTTPSetup.exe /VERYSILENT /NORESTART`
  - `WinCamHTTPSetup.exe /VERYSILENT /SUPPRESSMSGBOXES /LOG=install.log`

## Usage After Installation

1. **Configure settings (Admin):** Start Menu → *WinCamHTTP* → *WinCamHTTP Setup*
2. **Run the virtual camera (User):** Start Menu → *WinCamHTTP* → *WinCamHTTP*
3. **Verify in apps:** Select **"WinCamHTTP Source"** inside Windows Camera, Teams, Zoom, etc.

## Troubleshooting

- **ISCC.exe not found:** Install Inno Setup 6 or export the path via `INNO_SETUP_PATH`.
- **COM registration fails:** Confirm the installer was run elevated (required) or rerun `regsvr32 "C:\Program Files\WinCamHTTP\WinCamHTTPSource.dll"` manually.
- **Installer build fails:** Ensure Release/x64 binaries exist under `x64\Release` before compiling the `.iss`.

## Testing Notes

- Run `Installer\Output\WinCamHTTPSetup.exe /VERYSILENT /LOG=install.log` and confirm the log records a successful `regsvr32` execution.
- After uninstalling, re-run with `/LOG=uninstall.log` or monitor Event Viewer to ensure the `/u` invocation succeeds.
- If you need to validate manually, execute `regsvr32 /s "C:\Program Files\WinCamHTTP\WinCamHTTPSource.dll"` and confirm `MFCreateVirtualCamera` finds the virtual device in dependent apps.

## File Layout

```
Installer/
├── WinCamHTTP.iss      # Main Inno Setup script
├── Output/             # Generated installers (gitignored)
└── README.md           # This file
```

## Version History

- **v2.0.0** – Switched from WiX/MSI to Inno Setup 6 EXE installer
- **v1.0.0** – Original WiX/MSI implementation (removed)
