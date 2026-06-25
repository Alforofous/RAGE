# PowerShell counterpart of build.sh — configure and build RAGE on Windows from PowerShell.
#
# Usage (from the project root):
#   .\scripts\build.ps1
#
# Or, if your execution policy blocks unsigned scripts:
#   powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
#
# Native Windows tooling only — no bash, no WSL. Idempotent: re-configures only when the
# CMake cache is missing or stale, otherwise just rebuilds.

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

Write-Host '=== Building RAGE ===' -ForegroundColor Cyan

Invoke-CheckedNative 'git submodule update' {
    git -C $ProjectRoot submodule update --init
}

$cachePath = Join-Path $BuildDir 'CMakeCache.txt'
if (-not (Test-Path -LiteralPath $cachePath)) {
    Write-Host 'Configuring CMake...' -ForegroundColor Cyan
    Invoke-CheckedNative 'cmake configure' {
        cmake -S $ProjectRoot -B $BuildDir
    }
}

Invoke-CheckedNative 'cmake build' {
    cmake --build $BuildDir --target RAGE
}

Write-Host '=== Build succeeded ===' -ForegroundColor Green
