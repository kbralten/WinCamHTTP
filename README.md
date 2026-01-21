# WinCamHTTP

WinCamHTTP allows you to create virtual Windows webcams that stream video directly from a network URL (HTTP/MJPEG). This is useful for bringing IP cameras, network streams, or other HTTP video sources into applications like Zoom, Teams, Discord, or OBS that expect a standard webcam.

## Features

- **Network to Webcam:** Turn any MJPEG HTTP stream into a recognized Windows Camera source.
- **Multi-Camera Support:** Create multiple independent virtual webcams with different names and sources.
- **Easy Configuration:** Includes a Setup application to add, remove, and configure cameras easily.
- **Tray Application:** Simple system tray utility to manage the active virtual cameras.

## Installation

1.  Download the latest installer (`WinCamHTTPSetup.exe`) from the [**GitHub Releases**](https://github.com/kbralten/WinCamHTTP/releases/latest) page.
2.  Run the installer. *Note: Administrator privileges are required to register the virtual camera drivers.*
3.  Follow the on-screen instructions to complete the installation.

## How to Use

### 1. Configure Cameras
Run the **WinCamHTTP Setup** application (Administrator privileges required) to manage your cameras.
- Click **Add** to create a new virtual camera.
- **Friendly Name:** The name that will appear in apps like Zoom or Teams.
- **MJPEG URL:** The HTTP URL of the video stream (e.g., `http://192.168.1.50/video.mjpeg`).
- Click **Save** to apply changes.

### 2. Start the Service
Run the **WinCamHTTP** application from the Start menu.
- This runs in your system tray (near the clock).
- It ensures the virtual cameras are active and ready for use.

### 3. Use in Applications
Open your video conferencing or streaming software (Zoom, Teams, OBS, etc.). You will see your configured cameras (e.g., "WinCamHTTP Camera 1") listed alongside your physical webcams.

---

# Developer Notes

The following section is intended for developers contributing to the project or building from source.

## Project Structure

- `VCamSampleSource/` — Media Source DLL (virtual camera implementation) and COM registration logic
- `VCamSample/` — Setup application to add/remove/configure virtual cameras
- `WinCamHTTP/` — Tray application that starts/stops configured virtual cameras
- `Installer/` — Inno Setup script (`WinCamHTTP.iss`) and installer assets
- Build scripts: PowerShell scripts to build, register, and create installers

## Technical Overview (Multi-Camera Support)

- **Registry Layout:**
  - All cameras are stored under `HKLM\SOFTWARE\WinCamHTTP\Cameras`.
  - Each camera is a subkey named by a GUID and contains:
    - `CLSID` — COM class ID for the virtual camera implementation.
    - `FriendlyName` — Display name.
    - `Url` — MJPEG source URL.
    - `FrameRate` — Requested frame rate.

- **COM Registration:**
  - The DLL registers one or more COM class entries.
  - During registration (`DllRegisterServer`), the code enumerates `HKLM\SOFTWARE\WinCamHTTP\Cameras` and registers class information for every camera found.
  - The Activator/MediaSource code reads `HKLM` configuration based on the camera ID.

## Build & Registration Workflow

### Prerequisites
- Visual Studio 2022 (MSVC C++ workload)
- [Inno Setup 6](https://jrsoftware.org) (for building the installer)
- PowerShell

### 1. Build (Non-Elevated)
Restore packages and build the solution in Release mode:
```powershell
nuget restore WinCamHTTP.sln
msbuild WinCamHTTP.sln /p:Configuration=Release /p:Platform=x64 /m
```
Artifacts are placed under `x64\Release` for each project.

### 2. Manual Registration (Dev Loop)
To test without the full installer, use the registration script in an **Administrator** PowerShell:
```powershell
./build-and-register.ps1
```
This builds the solution, copies binaries to `C:\WinCamHTTP`, and registers the DLLs via `regsvr32`.

### 3. Build Installer
Run `./build-installer.ps1`. This compiles `Installer\WinCamHTTP.iss` and produces the setup EXE in `Installer\Output\`.

## Runtime Behavior & Troubleshooting
- The **Tray App** (`WinCamHTTP`) enumerates the registry and initializes the virtual cameras using `MFCreateVirtualCamera`.
- The **Setup App** (`VCamSample`) manages the registry keys in `HKLM`. Use `regedit` to inspect `HKLM\SOFTWARE\WinCamHTTP\Cameras` if settings seem incorrect.
- **Permissions:** Writing to HKLM and registering COM DLLs requires Admin privileges. Ensure the DLLs are in a location accessible by `LocalService` (e.g., `Program Files` or a dedicated generic folder like `C:\WinCamHTTP`).

## License

See the `LICENSE` file in the repository root.
