# WinCamHTTP — Multi-Camera Virtual Webcam Sample

WinCamHTTP is a Windows sample that implements a virtual webcam (Media Foundation virtual camera) which streams frames from an HTTP source. The project has been extended to support multiple virtual cameras (each with its own configuration and CLSID) and includes a setup UI, a tray application, and an MSI installer.

## Contents

- `VCamSampleSource/` — Media Source DLL (virtual camera implementation) and COM registration logic
- `VCamSample/` — Setup application to add/remove/configure virtual cameras
- `WinCamHTTP/` — Tray application that starts/stops configured virtual cameras
- `Installer/` — WiX project and installer assets
- Build scripts: PowerShell scripts to build, register, and create installers

## Key changes (multi-camera support)

- Per-camera registry layout:
  - All cameras are stored under `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras`.
  - Each camera is a subkey named by a GUID camera ID (string) and contains values:
    - `CLSID` — COM class ID for the virtual camera implementation (string)
    - `FriendlyName` — Display name for the camera (string)
    - `Url` — HTTP URL to fetch frames from (string)
    - `FrameRate` — Requested frame rate (DWORD)
    - Additional sample-specific configuration values as needed

- DLL/COM registration:
  - The DLL registers one or more COM class entries. During registration the code enumerates `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras` and registers class information for every camera found.
  - The Activator/MediaSource code expects a per-camera camera ID to be set before initialization so it can read its configuration from the registry.

## Developer workflow — build & registration

1. Prerequisites
   - Visual Studio (MSVC) with C++ workload
   - WiX Toolset (for building the installer). The repo includes local NuGet copies under `packages/`.
   - PowerShell (scripts included)

2. Build (non-elevated)
   - Open a Developer PowerShell or regular PowerShell with MSBuild on PATH.
   - Restore packages and build the solution:
     - `.
     build-and-register.ps1 -BuildOnly`
   - Output artifacts are placed under `x64\\Release` for each project.

3. Register DLLs and COM classes (requires admin)
   - Run the registration script from an elevated PowerShell to write registry keys and register the DLLs:
     - `.
     build-and-register.ps1 -Register`
   - Alternatively use `regsvr32` on the produced DLLs; make sure the DLLs are in a location accessible by the Frame Server services.

4. Installer
   - Use `create-installer.ps1` and the WiX project under `Installer/` to produce an MSI.

## Runtime behavior

- The `WinCamHTTP` tray app enumerates `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras`, and starts/stops a media source instance for each configured camera.
- The setup app (`VCamSample`) lets you add/edit/remove camera entries and writes them under `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras`.

## Troubleshooting and notes

- Administration: Writing to `HKLM` and registering COM DLLs requires Administrator privileges. Use an elevated PowerShell when running registration scripts or the installer.
- Multiple cameras: Each virtual camera must have a unique camera ID GUID and corresponding CLSID. The registry keys under `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras` are the single source of truth.
- Debugging registration issues: Use `regedit` to inspect `HKLM\\SOFTWARE\\WinCamHTTP\\Cameras`. Confirm the `CLSID` and `InprocServer32` entries are present for each registered class.
- Unregistering: Use `unregister.ps1` or `regsvr32 /u` for specific DLLs.

## Developer notes & next steps

- Add unit tests for configuration parsing and registry helpers where practical.
- Validate MSI on a clean Windows image to ensure registration and access rights work.
- Add CI (GitHub Actions) to build artifacts and produce installer packages.

## License

See the `LICENSE` file in the repository root.
