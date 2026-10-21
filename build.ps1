param(
    [switch]$Run,
    [switch]$ConfigDialog,
    [switch]$FullscreenScreensaver
)

$ErrorActionPreference = "Stop"

# Stop any running instance of ScreensaverStudio (.exe or .scr) so the binary isn't locked during build
Get-Process -Name "ScreensaverStudio*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Locate CMake from PATH or Visual Studio installation
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        $candidate = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) {
            $cmake = $candidate
        }
    }
}

if (-not $cmake) {
    Write-Error "CMake could not be found in PATH or Visual Studio installation."
    exit 1
}

Write-Host "Using CMake: $cmake" -ForegroundColor Cyan

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

& $cmake -S . -B build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exePath = Join-Path $PSScriptRoot "build\Release\ScreensaverStudio.exe"
$scrPath = Join-Path $PSScriptRoot "build\Release\ScreensaverStudio.scr"

Write-Host "`nBuild succeeded!" -ForegroundColor Green
Write-Host "  Studio Executable : $exePath"
Write-Host "  Windows .SCR File : $scrPath"

if ($ConfigDialog) {
    Start-Process $scrPath -ArgumentList "/c"
} elseif ($FullscreenScreensaver) {
    Start-Process $scrPath -ArgumentList "/s"
} elseif ($Run) {
    Start-Process $exePath
}
