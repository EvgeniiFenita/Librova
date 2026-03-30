param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [switch]$SkipConfigure,
    [switch]$SkipNative,
    [switch]$SkipManaged
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$nativePresetRoot = Join-Path $repoRoot "out\build\$Preset"
$hostExecutable = Join-Path $nativePresetRoot "apps\Librova.Core.Host\$Configuration\LibrovaCoreHostApp.exe"
$uiTestsProject = Join-Path $repoRoot "tests\Librova.UI.Tests\Librova.UI.Tests.csproj"

if (-not $SkipNative) {
    if (-not $SkipConfigure) {
        Write-Host "==> Configuring native build ($Preset)"
        cmake --preset $Preset
    }

    Write-Host "==> Building native code ($Configuration)"
    cmake --build --preset $Preset --config $Configuration

    Write-Host "==> Running native tests ($Configuration)"
    ctest --test-dir $nativePresetRoot -C $Configuration --output-on-failure
}

if (-not $SkipManaged) {
    if (Test-Path -LiteralPath $hostExecutable) {
        $env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExecutable
    }

    Write-Host "==> Building managed UI tests ($Configuration)"
    dotnet build $uiTestsProject -c $Configuration

    Write-Host "==> Running managed UI tests ($Configuration)"
    dotnet test $uiTestsProject --no-build -c $Configuration
}

Write-Host "==> All requested tests completed successfully."
