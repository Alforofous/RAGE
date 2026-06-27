# PowerShell counterpart of build.sh — configure and build RAGE on Windows from PowerShell.
#
# Usage (from the project root):
#   .\scripts\build.ps1                  # production build (default): no Tracy, no dev-only UI
#   .\scripts\build.ps1 -Dev             # dev build: Tracy linked + debug UI dev features
#
# Or, if your execution policy blocks unsigned scripts:
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 [-Dev]
#
# Native Windows tooling only — no bash, no WSL. CMake is always re-invoked so flipping
# -Dev between runs correctly updates the cache.

param([switch] $Dev)

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
$DevFlag = if ($Dev) { 'ON' } else { 'OFF' }
$ProfileLabel = if ($Dev) { 'dev' } else { 'production' }

Write-Host "=== Building RAGE ($ProfileLabel) ===" -ForegroundColor Cyan

Invoke-CheckedNative 'git submodule update' {
    git -C $ProjectRoot submodule update --init
}

Write-Host 'Configuring CMake...' -ForegroundColor Cyan
Invoke-CheckedNative 'cmake configure' {
    cmake -S $ProjectRoot -B $BuildDir "-DRAGE_DEV_BUILD=$DevFlag"
}

Invoke-CheckedNative 'cmake build' {
    cmake --build $BuildDir --target RAGE
}

Write-Host '=== Build succeeded ===' -ForegroundColor Green
