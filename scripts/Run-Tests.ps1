param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [string]$QtRoot = "C:\Qt\6.11.0\msvc2022_64",
    [switch]$SkipConfigure,
    [switch]$SkipNative,
    [switch]$SkipBoundaryValidation,
    [int]$ParallelJobs = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS)
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot "out\build\$Preset"

function Assert-LastExitCode([string]$stepName) {
    if ($LASTEXITCODE -ne 0) {
        throw "$stepName failed with exit code $LASTEXITCODE."
    }
}

Push-Location $repoRoot
try {
    if (-not $env:QT_ROOT -and (Test-Path -LiteralPath $QtRoot)) {
        $env:QT_ROOT = $QtRoot
    }

    if ($env:QT_ROOT) {
        $qtBin = Join-Path $env:QT_ROOT "bin"
        if ($env:PATH -notlike "*$qtBin*") {
            $env:PATH = "$qtBin;$env:PATH"
        }
    }
    else {
        throw "Qt not found. Set QT_ROOT env var or pass -QtRoot parameter."
    }

    if (-not $SkipNative) {
        if (-not $SkipConfigure) {
            Write-Host "==> Configuring native + Qt build ($Preset)"
            cmake --preset $Preset
            Assert-LastExitCode "CMake configure"
        }

        Write-Host "==> Building native + Qt targets ($Configuration)"
        cmake --build --preset $Preset --config $Configuration
        Assert-LastExitCode "Build"

        Write-Host "==> Running native + Qt tests ($Configuration, -j $ParallelJobs)"
        ctest --test-dir $buildRoot -C $Configuration --output-on-failure -j $ParallelJobs
        Assert-LastExitCode "CTest"
    }

    if (-not $SkipBoundaryValidation) {
        Write-Host "==> Running boundary validation"
        & "$PSScriptRoot\ValidateBoundary.ps1"
        Assert-LastExitCode "Boundary validation"
    }

    Write-Host "==> All requested checks completed successfully."
}
finally {
    Pop-Location
}
