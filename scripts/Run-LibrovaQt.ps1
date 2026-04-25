param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [string]$QtRoot = "C:\Qt\6.11.0\msvc2022_64",
    [switch]$NoLaunch,
    [switch]$FirstRun,
    [switch]$SecondRun,
    [switch]$StartupErrorRecovery
)

if ($StartupErrorRecovery -and -not $SecondRun) {
    $SecondRun = $true
}

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$runtimeRoot = Join-Path $repoRoot "out\runtime\qt"
$libraryRoot = Join-Path $runtimeRoot "library"
$logFile = Join-Path $runtimeRoot "librova-qt.log"
$preferencesFile = Join-Path $runtimeRoot "preferences.json"
$shellStateFile = Join-Path $runtimeRoot "shell-state.json"
$qtExecutable = Join-Path $repoRoot "out\build\$Preset\apps\Librova.Qt\$Configuration\LibrovaQtApp.exe"
$startupProbeDelayMilliseconds = 2000

function Write-QtLogTail([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Host "    log: (not found) $path"
        return
    }
    Write-Host "    log: $path"
    Get-Content -LiteralPath $path -Tail 40 | ForEach-Object { Write-Host "      $_" }
}

Push-Location $repoRoot
try {
    # Make Qt discoverable by CMake
    if (-not $env:QT_ROOT -and (Test-Path $QtRoot)) {
        $env:QT_ROOT = $QtRoot
    }

    if (-not $env:QT_ROOT) {
        throw "Qt not found. Set QT_ROOT env var or pass -QtRoot parameter."
    }

    # Add Qt bin to PATH so Qt DLLs are found at runtime
    $qtBin = Join-Path $env:QT_ROOT "bin"
    if ($env:PATH -notlike "*$qtBin*") {
        $env:PATH = "$qtBin;$env:PATH"
    }

    New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
    if (-not $FirstRun -and -not $SecondRun) {
        New-Item -ItemType Directory -Force -Path $libraryRoot | Out-Null
    }

    Write-Host "==> Configuring ($Preset) with QT_ROOT=$env:QT_ROOT"
    cmake --preset $Preset

    Write-Host "==> Building LibrovaQtApp ($Configuration)"
    cmake --build --preset $Preset --config $Configuration --target LibrovaQtApp

    if (-not (Test-Path -LiteralPath $qtExecutable)) {
        throw "Qt executable was not found: $qtExecutable"
    }

    Write-Host "==> Launching LibrovaQtApp"
    Write-Host "    exe:         $qtExecutable"
    Write-Host "    log:         $logFile"
    Write-Host "    preferences: $preferencesFile"
    Write-Host "    shell state: $shellStateFile"
    if ($FirstRun) {
        Write-Host "    library:     (not preset; first-run setup should appear)"
    }
    elseif ($SecondRun) {
        Write-Host "    library:     (not preset; app will use saved preferences)"
    }
    else {
        Write-Host "    library:     $libraryRoot"
    }

    if ($NoLaunch) {
        Write-Host "==> NoLaunch specified, skipping process start."
        return
    }

    $launchArgs = @(
        "--log-file", $logFile,
        "--preferences-file", $preferencesFile,
        "--shell-state-file", $shellStateFile
    )

    if (-not $FirstRun -and -not $SecondRun) {
        $launchArgs += @("--library-root", $libraryRoot)
        if (-not (Test-Path -LiteralPath (Join-Path $libraryRoot "Database\librova.db"))) {
            $launchArgs += "--create-library-root"
        }
    }

    if ($FirstRun) {
        foreach ($f in @($preferencesFile, $shellStateFile, $logFile)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
        }
    }

    $process = Start-Process -FilePath $qtExecutable -ArgumentList $launchArgs `
        -WorkingDirectory $repoRoot -PassThru
    Start-Sleep -Milliseconds $startupProbeDelayMilliseconds
    $process.Refresh()

    if ($process.HasExited) {
        Write-Host ("==> LibrovaQtApp exited during startup. PID: {0}" -f $process.Id)
        if ($process.ExitCode -ne 0) {
            Write-Host ("    Exit code: {0}" -f $process.ExitCode)
        }
        Write-QtLogTail $logFile
        throw "LibrovaQtApp exited before startup completed."
    }

    Write-Host ("==> LibrovaQtApp started. PID: {0}" -f $process.Id)
}
finally {
    Pop-Location
}
