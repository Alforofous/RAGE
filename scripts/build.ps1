# PowerShell counterpart of build.sh — configure and build RAGE on Windows from PowerShell.
#
# Usage (from the project root):
#   .\scripts\build.ps1                  # lean build, no profiling
#   .\scripts\build.ps1 -Profile         # build with Tracy profiling enabled
#
# Or, if your execution policy blocks unsigned scripts:
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 [-Profile]
#
# Native Windows tooling only — no bash, no WSL. CMake is always re-invoked so toggling
# -Profile between runs correctly updates the cache.

param([switch] $Profile)

$ErrorActionPreference = 'Stop'

function Invoke-CheckedNative {
    param([string] $Label, [scriptblock] $Action)
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$BuildDir = Join-Path $ProjectRoot 'build'
$ProfileFlag = if ($Profile) { 'ON' } else { 'OFF' }

Write-Host "=== Building RAGE (profiling=$ProfileFlag) ===" -ForegroundColor Cyan

Invoke-CheckedNative 'git submodule update' {
    git -C $ProjectRoot submodule update --init
}

Write-Host 'Configuring CMake...' -ForegroundColor Cyan
Invoke-CheckedNative 'cmake configure' {
    cmake -S $ProjectRoot -B $BuildDir "-DRAGE_ENABLE_PROFILING=$ProfileFlag"
}

Invoke-CheckedNative 'cmake build' {
    cmake --build $BuildDir --target RAGE
}

Write-Host '=== Build succeeded ===' -ForegroundColor Green
