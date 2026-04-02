param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [switch]$NoLaunch,
    [switch]$FirstRun,
    [switch]$SecondRun,
    [switch]$StartupErrorRecovery
)

if ($StartupErrorRecovery -and -not $SecondRun) {
    $SecondRun = $true
}

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$runtimeRoot = Join-Path $repoRoot "out\runtime"
$libraryRoot = Join-Path $runtimeRoot "library"
$bootstrapUiLogFile = Join-Path $runtimeRoot "bootstrap-ui.log"
$libraryUiLogFile = Join-Path $libraryRoot "Logs\ui.log"
$uiStateFile = Join-Path $runtimeRoot "ui-shell-state.json"
$uiPreferencesFile = Join-Path $runtimeRoot "ui-preferences.json"
$hostExecutable = Join-Path $repoRoot "out\build\$Preset\apps\Librova.Core.Host\$Configuration\LibrovaCoreHostApp.exe"
$uiExecutable = Join-Path $repoRoot "out\dotnet\bin\Librova.UI\$Configuration\net10.0\Librova.UI.exe"

New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
if (-not $FirstRun -and -not $SecondRun) {
    New-Item -ItemType Directory -Force -Path $libraryRoot | Out-Null
}

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

$env:LIBROVA_UI_STATE_FILE = $uiStateFile
$env:LIBROVA_UI_PREFERENCES_FILE = $uiPreferencesFile
$env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExecutable

if ($FirstRun) {
    if (Test-Path -LiteralPath $uiStateFile) {
        Remove-Item -LiteralPath $uiStateFile -Force
    }

    if (Test-Path -LiteralPath $uiPreferencesFile) {
        Remove-Item -LiteralPath $uiPreferencesFile -Force
    }

    if (Test-Path -LiteralPath $bootstrapUiLogFile) {
        Remove-Item -LiteralPath $bootstrapUiLogFile -Force
    }

    $env:LIBROVA_UI_LOG_FILE = $bootstrapUiLogFile
    Remove-Item Env:LIBROVA_LIBRARY_ROOT -ErrorAction SilentlyContinue
}
elseif ($SecondRun) {
    $env:LIBROVA_UI_LOG_FILE = $bootstrapUiLogFile
    Remove-Item Env:LIBROVA_LIBRARY_ROOT -ErrorAction SilentlyContinue
}
else {
    $env:LIBROVA_UI_LOG_FILE = $libraryUiLogFile
    $env:LIBROVA_LIBRARY_ROOT = $libraryRoot
}

Write-Host "==> Launching Librova UI"
if ($FirstRun -or $SecondRun) {
    Write-Host "    UI log:      (bootstrap) $bootstrapUiLogFile -> (final) <LibraryRoot>\\Logs\\ui.log"
}
else {
    Write-Host "    UI log:      $libraryUiLogFile"
}
Write-Host "    UI state:    $uiStateFile"
Write-Host "    UI prefs:    $uiPreferencesFile"
if ($FirstRun) {
    Write-Host "    Library root:(not preset; first-run setup should appear)"
}
elseif ($SecondRun) {
    Write-Host "    Library root:(not preset; app will use saved preferences if present)"
}
else {
    Write-Host "    Library root:$libraryRoot"
}
Write-Host "    Host exe:    $hostExecutable"

if ($NoLaunch) {
    Write-Host "==> NoLaunch specified, skipping UI process start."
    return
}

$process = Start-Process -FilePath $uiExecutable -PassThru
Write-Host ("==> Librova UI started. PID: {0}" -f $process.Id)
