Repository onboarding for Copilot coding agent

Purpose
- Give a new coding agent the minimal, validated information needed to implement and build code changes reliably in this repo.
- Short, actionable instructions only (<= 2 pages). Trust these instructions; search only if something is missing or produces a failure.

Summary
- This repo implements a Windows virtual camera (Media Foundation Virtual Camera) that streams from an MJPEG HTTP source.
- Projects:
  - `VCamSampleSource` (native C++ DLL): Media Source (virtual camera) loaded by the Windows Frame Server.
  - `VCamSample` (native C++ exe): Setup application (Admin) to configure camera settings in the registry (HKLM).
  - `WinCamHTTP` (native C++ exe): Tray app (user) that starts/stops virtual camera(s).
  - `Installer` (WiX) and scripts for building an MSI/installer.
- Languages: C++ (MSVC), Win32 API, Media Foundation, WinRT/wil.
- Repository size: small-to-medium (~many C++ source files); primary dev/test is Windows-only.

Key goals for an agent
- Avoid producing code that breaks MSVC builds on Release|x64.
- Do not attempt to modify or read registry keys without explicit admin intent in the change.
- Prefer minimal, focused patches; avoid reformatting large areas.

Build and validation (validated steps)
Prerequisites (always ensure these are available):
- Windows machine with Visual Studio 2022 Community (MSVC toolset) installed.
- MSBuild path (Typical): `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
- PowerShell available (v5.1 or later).

Non-elevated build (use this to compile and run tests locally):
1. From repository root:
   - Ensure NuGet is present or let the script download it:
     `if (-not (Test-Path .\nuget.exe)) { Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile .\nuget.exe }`
   - Restore packages (non-admin):
     `.\nuget.exe restore .\WinCamHTTP.sln`
   - Build the solution (non-admin):
     `& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" .\WinCamHTTP.sln /p:Configuration=Release /p:Platform=x64 /m /v:m`
   - Successful build outputs: `x64\Release\WinCamHTTPSource.dll`, `WinCamHTTPSetup.exe`, `WinCamHTTP.exe`.

Administrator-only registration/copy steps
- The virtual camera DLL must be registered under `HKLM` so that system Frame Server services can load it. Registration and copying to a system-accessible folder require Administrator privileges.
- Use the provided script with elevation:
  - Run an elevated PowerShell (Run as Administrator) and execute:
    `.\build-and-register.ps1` (defaults to Release x64 and copies to `C:\WinCamHTTP` and runs `regsvr32`)
  - If you change camera registrations (add/remove camera IDs in the registry), re-run registration (or re-run the script with `-SkipBuild` to only copy/register).

Common pitfalls and how to avoid them
- Do not attempt to run `build-and-register.ps1` in a non-elevated session — it will abort with `#Requires -RunAsAdministrator`.
- If you change registry layout (HKLM keys) update both the Setup app and Media Source to read/write the same keys; the Frame Server runs as system so the DLL must use HKLM.
- When adding new public APIs or headers, keep include ordering consistent and avoid adding heavy folder-wide changes.
- Avoid changing default COM CLSIDs or registration structure unless you update `dllmain.cpp` and `DllRegisterServer` accordingly.

Project layout & important files
- `WinCamHTTP.sln` - Visual Studio solution referencing 3 main projects.
- `VCamSampleSource/` - Media Source (DLL). Key files:
  - `dllmain.cpp` - COM registration, ClassFactory, multi-camera mapping logic.
  - `Activator.*` - Activator that creates the MediaSource.
  - `MediaSource.h/.cpp` - Main Media Source implementation (reads config from HKLM\SOFTWARE\WinCamHTTP\Cameras\<CameraID>).
  - `MediaStream.*` - Per-stream behavior.
- `VCamSample/` - Setup app (Win32 GUI). Key files:
  - `VCamSample.cpp` - UI, list-based camera management, registry changes.
  - Uses `HKLM\SOFTWARE\WinCamHTTP\Cameras\<CameraID>` keys per camera.
- `WinCamHTTP/` - Tray app (Win32 GUI). Key files:
  - `WinCamHTTP.cpp` - Loads camera entries from registry and starts all virtual cameras via `MFCreateVirtualCamera`.
- `build-and-register.ps1` - High-level script to build, copy outputs to `C:\WinCamHTTP`, and register the DLL (requires Admin).

Checks / CI assumptions
- The repo doesn't include GitHub Actions workflows by default; however, changes must compile with MSVC and not depend on local-only files.
- If you create new native code, ensure it compiles for `Release|x64` and does not add new external DLL dependencies unless necessary.

When to search the codebase
- Trust this file for: build commands, where to register, registry layout, and main code locations.
- Search the repo when you need to find an exact symbol or ref (e.g., `MFCreateVirtualCamera`, `HKLM\SOFTWARE\WinCamHTTP`) or when the change touches many areas.

If build fails
- Re-run the exact MSBuild command above. Capture compile errors and fix one file at a time.
- For DLL registration errors: ensure file is copied to a directory accessible by `LocalService`/`LocalSystem` (avoid per-user build output locations) before running `regsvr32`.

Final notes to the coding agent
- Always run the non-elevated build command first to validate compilation.
- Only run registration or copy steps in an elevated PowerShell session.
- Keep patches small and focused; run the build after each native code file change.
- If anything in this file conflicts with a discovered repository file, re-run a search (but prefer trusting these instructions first).

End of onboarding instructions
