param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$runtimeRoot = Join-Path $repoRoot "out\runtime"
$runtimeLogs = Join-Path $runtimeRoot "logs"
$libraryRoot = Join-Path $runtimeRoot "library"
$uiLogFile = Join-Path $runtimeLogs "ui.log"
$uiStateFile = Join-Path $runtimeRoot "ui-shell-state.json"
$uiPreferencesFile = Join-Path $runtimeRoot "ui-preferences.json"
$hostExecutable = Join-Path $repoRoot "out\build\$Preset\apps\Librova.Core.Host\$Configuration\LibrovaCoreHostApp.exe"
$uiExecutable = Join-Path $repoRoot "out\dotnet\bin\Librova.UI\$Configuration\net10.0\Librova.UI.exe"

New-Item -ItemType Directory -Force -Path $runtimeLogs | Out-Null
New-Item -ItemType Directory -Force -Path $libraryRoot | Out-Null

Write-Host "==> Configuring native build ($Preset)"
cmake --preset $Preset

Write-Host "==> Building native host ($Configuration)"
cmake --build --preset $Preset --config $Configuration

Write-Host "==> Building UI ($Configuration)"
dotnet build (Join-Path $repoRoot "apps\Librova.UI\Librova.UI.csproj") -c $Configuration

if (-not (Test-Path -LiteralPath $hostExecutable)) {
    throw "Native host executable was not found: $hostExecutable"
}

if (-not (Test-Path -LiteralPath $uiExecutable)) {
    throw "UI executable was not found: $uiExecutable"
}

$env:LIBROVA_UI_LOG_FILE = $uiLogFile
$env:LIBROVA_UI_STATE_FILE = $uiStateFile
$env:LIBROVA_UI_PREFERENCES_FILE = $uiPreferencesFile
$env:LIBROVA_LIBRARY_ROOT = $libraryRoot
$env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExecutable

Write-Host "==> Launching Librova UI"
Write-Host "    UI log:      $uiLogFile"
Write-Host "    UI state:    $uiStateFile"
Write-Host "    UI prefs:    $uiPreferencesFile"
Write-Host "    Library root:$libraryRoot"
Write-Host "    Host exe:    $hostExecutable"

if ($NoLaunch) {
    Write-Host "==> NoLaunch specified, skipping UI process start."
    return
}

& $uiExecutable
