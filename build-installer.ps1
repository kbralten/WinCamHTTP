<#
.SYNOPSIS
    Builds WinCamHTTP solution and creates an Inno Setup installer.

.DESCRIPTION
    This script performs the complete build and packaging for WinCamHTTP:
    1. Builds the main solution in Release x64 configuration
    2. Locates the Inno Setup compiler (ISCC.exe)
    3. Compiles Installer\WinCamHTTP.iss into WinCamHTTPSetup.exe
    4. Copies the installer next to the build outputs for convenience

.PARAMETER Configuration
    Build configuration (default: Release)
    
.PARAMETER Platform
    Build platform (default: x64)
    
.PARAMETER SkipBuild
    Skip main solution build and only create installer

.PARAMETER InnoSetupPath
    Optional explicit path to ISCC.exe. If not supplied, common install paths
    (including %LOCALAPPDATA%\Programs\Inno Setup 6) will be probed.
#>

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$SkipBuild,
    [string]$InnoSetupPath
)

# Script variables
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SolutionFile = Join-Path $RepoRoot "WinCamHTTP.sln"
$OutputDir = Join-Path $RepoRoot "$Platform\$Configuration"
$MSBuildPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
$InnoScript = Join-Path $RepoRoot "Installer\WinCamHTTP.iss"
$InstallerOutputDir = Join-Path $RepoRoot "Installer\Output"

Write-Host "WinCamHTTP Inno Setup Build Script" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Green
Write-Host "Repository: $RepoRoot"
Write-Host "Configuration: $Configuration $Platform"
Write-Host ""

# Function to check if file exists, exit if not
function Test-FileExists {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path $Path)) {
        Write-Error "$Description not found at: $Path"
        exit 1
    }
}

# Function to run command and check exit code
function Invoke-External {
    param([string]$Command, [string[]]$Arguments, [string]$Description)
    Write-Host "Running: $Description..." -ForegroundColor Yellow
    $process = Start-Process -FilePath $Command -ArgumentList $Arguments -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        Write-Error "$Description failed with exit code $($process.ExitCode)"
        exit 1
    }
    Write-Host "$Description completed successfully." -ForegroundColor Green
}

function Resolve-IsccPath {
    $candidates = @()
    if ($InnoSetupPath) { $candidates += $InnoSetupPath }
    if ($env:INNO_SETUP_PATH) { $candidates += $env:INNO_SETUP_PATH }
    $candidates += @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe",
        "C:\Users\kevin\AppData\Local\Programs\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    Write-Host "Inno Setup compiler (ISCC.exe) not found." -ForegroundColor Red
    Write-Host "Install Inno Setup 6 or pass -InnoSetupPath 'C:\Path\To\ISCC.exe'." -ForegroundColor Yellow
    Write-Host "Download: https://jrsoftware.org/isinfo.php" -ForegroundColor Yellow
    exit 1
}

try {
    # Step 1: Check prerequisites
    Write-Host "[1/4] Checking prerequisites..." -ForegroundColor Cyan
    Test-FileExists $SolutionFile "Solution file"
    Test-FileExists $MSBuildPath "MSBuild"
    Test-FileExists $InnoScript "Inno Setup script"
    $IsccPath = Resolve-IsccPath
    Write-Host "Using Inno Setup compiler at: $IsccPath" -ForegroundColor Green
    
    # Step 2: Build main solution
    if (-not $SkipBuild) {
        Write-Host "[2/4] Building main solution..." -ForegroundColor Cyan
        $buildArgs = @(
            $SolutionFile,
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform",
            "/m",
            "/verbosity:minimal"
        )
        Invoke-External $MSBuildPath $buildArgs "Main solution build"
        
        # Verify outputs exist
        $requiredFiles = @(
            "WinCamHTTPSource.dll",
            "WinCamHTTPSetup.exe", 
            "WinCamHTTP.exe"
        )
        
        foreach ($file in $requiredFiles) {
            $filePath = Join-Path $OutputDir $file
            Test-FileExists $filePath "Build output: $file"
        }
    } else {
        Write-Host "[2/4] Skipping main solution build (SkipBuild specified)" -ForegroundColor Yellow
    }
    
    # Step 3: Compile Inno Setup installer
    Write-Host "[3/4] Compiling Inno Setup installer..." -ForegroundColor Cyan
    $installerArgs = @(
        "/DConfiguration=$Configuration",
        "/DPlatform=$Platform",
        "`"$InnoScript`""
    )
    Invoke-External $IsccPath $installerArgs "Inno Setup compile"
    
    # Step 4: Verify and display results
    Write-Host "[4/4] Verifying installer output..." -ForegroundColor Cyan
    $installerPath = Join-Path $InstallerOutputDir "WinCamHTTPSetup.exe"
    Test-FileExists $installerPath "Installer package"

    $destDir = Join-Path $RepoRoot "$Platform\$Configuration"
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir | Out-Null
    }
    $destInstaller = Join-Path $destDir "WinCamHTTPSetup.exe"
    Copy-Item $installerPath $destInstaller -Force
    
    # Get installer file info
    $installerInfo = Get-Item $installerPath
    $installerSizeMB = [math]::Round($installerInfo.Length / 1MB, 2)
    
    Write-Host ""
    Write-Host "SUCCESS: WinCamHTTP installer created!" -ForegroundColor Green
    Write-Host "====================================" -ForegroundColor Green
    Write-Host "Installer package: $installerPath"
    Write-Host "Package size: $installerSizeMB MB"
    Write-Host "Created: $($installerInfo.CreationTime)"
    Write-Host ""
    Write-Host "Installation Features:" -ForegroundColor White
    Write-Host "• Installs to: C:\\Program Files\\WinCamHTTP\\"
    Write-Host "• Registers COM components automatically"
    Write-Host "• Creates Start Menu shortcuts"
    Write-Host "• Supports silent installation: WinCamHTTPSetup.exe /VERYSILENT"
    Write-Host "• Supports uninstall via Programs & Features"
    Write-Host ""
    Write-Host "To test the installer:" -ForegroundColor Yellow
    Write-Host "1. Double-click WinCamHTTPSetup.exe"
    Write-Host "2. Follow the installation wizard"
    Write-Host "3. Launch from Start Menu → WinCamHTTP"
    Write-Host ""
    
} catch {
    Write-Error "Build script failed: $($_.Exception.Message)"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}