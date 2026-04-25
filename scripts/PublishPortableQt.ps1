#
# PublishPortableQt.ps1 — package the Qt single-process Librova build into a
# portable, self-contained directory using windeployqt.
#
# Output layout:
#   <OutputDir>\
#       LibrovaQtApp.exe
#       Qt6Core.dll, Qt6Gui.dll, Qt6Qml.dll, ...   (deployed by windeployqt)
#       qml\                                        (QML imports)
#       plugins\                                    (Qt plugins)
#       PortableData\                               (created on first run)
#
# Usage:
#   scripts\PublishPortableQt.ps1
#   scripts\PublishPortableQt.ps1 -Configuration Release -OutputDir out\publish\Librova-Qt
#
param(
    [string]$Preset        = "x64-debug",
    [string]$Configuration = "Debug",
    [string]$QtRoot        = "C:\Qt\6.11.0\msvc2022_64",
    [string]$OutputDir,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) {
    $OutputDir = Join-Path $repoRoot "out\publish\Librova-Qt-$Configuration"
}

Push-Location $repoRoot
try {
    if (-not $env:QT_ROOT -and (Test-Path $QtRoot)) {
        $env:QT_ROOT = $QtRoot
    }
    if (-not $env:QT_ROOT) {
        throw "Qt not found. Set QT_ROOT env var or pass -QtRoot."
    }

    $qtBin       = Join-Path $env:QT_ROOT "bin"
    $windeployqt = Join-Path $qtBin "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt)) {
        throw "windeployqt.exe not found at: $windeployqt"
    }
    if ($env:PATH -notlike "*$qtBin*") {
        $env:PATH = "$qtBin;$env:PATH"
    }

    if (-not $SkipBuild) {
        Write-Host "==> Configuring ($Preset) with QT_ROOT=$env:QT_ROOT"
        cmake --preset $Preset
        Write-Host "==> Building LibrovaQtApp ($Configuration)"
        cmake --build --preset $Preset --config $Configuration --target LibrovaQtApp
    }

    $exePath = Join-Path $repoRoot "out\build\$Preset\apps\Librova.Qt\$Configuration\LibrovaQtApp.exe"
    if (-not (Test-Path -LiteralPath $exePath)) {
        throw "LibrovaQtApp.exe not found: $exePath"
    }

    if (Test-Path -LiteralPath $OutputDir) {
        Write-Host "==> Cleaning $OutputDir"
        Remove-Item -LiteralPath $OutputDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

    Write-Host "==> Copying LibrovaQtApp.exe to $OutputDir"
    Copy-Item -LiteralPath $exePath -Destination $OutputDir -Force

    # Copy any sibling DLLs produced by the build (e.g. vcpkg deps).
    $exeDir = Split-Path -Parent $exePath
    Get-ChildItem -LiteralPath $exeDir -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $OutputDir -Force
    }

    $qmlSourceDir = Join-Path $repoRoot "apps\Librova.Qt\qml"
    $deployedExe  = Join-Path $OutputDir "LibrovaQtApp.exe"

    Write-Host "==> Running windeployqt"
    $argsList = @(
        "--qmldir", $qmlSourceDir,
        "--no-translations",
        "--no-system-d3d-compiler",
        "--no-opengl-sw"
    )
    if ($Configuration -eq "Release") {
        $argsList += "--release"
    } else {
        $argsList += "--debug"
    }
    $argsList += $deployedExe

    & $windeployqt @argsList
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }

    # Mark this build as portable so the runtime puts data under <OutputDir>\PortableData.
    $portableMarker = Join-Path $OutputDir "PortableData"
    New-Item -ItemType Directory -Force -Path $portableMarker | Out-Null

    Write-Host ""
    Write-Host "==> Portable Qt build published:"
    Write-Host "    $deployedExe"
    Write-Host "    Data root: $portableMarker (will be populated on first run)"
}
finally {
    Pop-Location
}
