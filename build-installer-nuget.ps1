# WiX v4 build script using NuGet packages (no global WiX install required)

param(
	[string]$Configuration = 'Release',
	[string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

Write-Host "Building WinCamHTTP MSI (WiX v4) - $Configuration $Platform" -ForegroundColor Cyan

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$solution = Join-Path $repo 'WinCamHTTP.sln'
$installerProj = Join-Path $repo 'Installer\Installer.wixproj'

# 1) Build main binaries first
Write-Host '[1/3] Building main solution...' -ForegroundColor Yellow
& msbuild $solution /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m
if ($LASTEXITCODE -ne 0) { throw 'Main solution build failed.' }

# 2) Restore and build WiX v4 installer (SDK-style)
Write-Host '[2/3] Restoring and building WiX v4 installer...' -ForegroundColor Yellow
& dotnet restore $installerProj
if ($LASTEXITCODE -ne 0) { throw 'Installer restore failed.' }

& dotnet build $installerProj -c $Configuration -p:Platform=$Platform -v:m
if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }

# 3) Copy MSI to canonical output
$msiSource = Join-Path $repo "Installer\bin\$Configuration\WinCamHTTPSetup.msi"
if (-not (Test-Path $msiSource)) {
	# WiX v4 default might output under obj; try to locate
	$msiSource = Get-ChildItem -Path (Join-Path $repo 'Installer') -Filter 'WinCamHTTPSetup.msi' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $msiSource) { throw 'MSI output not found.' }

$destDir = Join-Path $repo "$Platform\$Configuration"
if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir | Out-Null }
$destMsi = Join-Path $destDir 'WinCamHTTPSetup.msi'
Copy-Item $msiSource $destMsi -Force

$info = Get-Item $destMsi
Write-Host "[Done] MSI: $($info.FullName) ($([math]::Round($info.Length/1MB,2)) MB)" -ForegroundColor Green
