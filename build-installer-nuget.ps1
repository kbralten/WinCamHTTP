param(
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64',
    [string]$InnoSetupPath
)

$ErrorActionPreference = 'Stop'

Write-Host "Building WinCamHTTP + Inno installer - $Configuration $Platform" -ForegroundColor Cyan

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$solution = Join-Path $repo 'WinCamHTTP.sln'
$innoScript = Join-Path $repo 'Installer\WinCamHTTP.iss'
$binDir = Join-Path $repo "$Platform\$Configuration"
$outputExe = Join-Path $repo 'Installer\Output\WinCamHTTPSetup.exe'

function Resolve-Iscc {
    param([string]$Explicit)

    $candidates = @()
    if ($Explicit) { $candidates += $Explicit }
    if ($env:INNO_SETUP_PATH) { $candidates += $env:INNO_SETUP_PATH }
    $candidates += @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw 'ISCC.exe not found. Install Inno Setup 6 or specify -InnoSetupPath.'
}

$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if (-not $msbuild) { throw 'msbuild not found on PATH. Run from Developer Command Prompt.' }
$msbuildPath = $msbuild.Source

# 1) Build main binaries
Write-Host '[1/3] Building main solution...' -ForegroundColor Yellow
& $msbuildPath $solution /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m
if ($LASTEXITCODE -ne 0) { throw 'Main solution build failed.' }

# Basic verification
$required = @('WinCamHTTP.exe','WinCamHTTPSetup.exe','WinCamHTTPSource.dll')
foreach ($file in $required) {
    $testPath = Join-Path $binDir $file
    if (-not (Test-Path $testPath)) {
        throw "Required binary missing: $testPath"
    }
}

# 2) Compile Inno Setup installer
Write-Host '[2/3] Compiling installer...' -ForegroundColor Yellow
$iscc = Resolve-Iscc -Explicit $InnoSetupPath
Write-Host "Using ISCC: $iscc" -ForegroundColor Green
& $iscc "/DConfiguration=$Configuration" "/DPlatform=$Platform" "`"$innoScript`""
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }

# 3) Report results
Write-Host '[3/3] Installer ready' -ForegroundColor Green
if (Test-Path $outputExe) {
    $info = Get-Item $outputExe
    Write-Host "Installer: $($info.FullName) ($([math]::Round($info.Length/1MB,2)) MB)" -ForegroundColor Green
    Write-Host "SHA256: $((Get-FileHash $outputExe -Algorithm SHA256).Hash)"
} else {
    Write-Warning 'Installer output not found in Installer\\Output.'
}
