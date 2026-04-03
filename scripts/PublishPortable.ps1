param(
    [string]$NativePreset = "x64-release-static",
    [string]$Configuration = "Release",
    [string]$RuntimeIdentifier = "win-x64",
    [string]$PackageName = "win-x64-portable",
    [switch]$SkipConfigure,
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$packageRoot = Join-Path $repoRoot "out\package\$PackageName"
$publishRoot = Join-Path $repoRoot "out\publish\Librova.UI\$Configuration\$RuntimeIdentifier"
$nativeOutputRoot = Join-Path $repoRoot "out\build\$NativePreset\apps\Librova.Core.Host\$Configuration"
$uiProject = Join-Path $repoRoot "apps\Librova.UI\Librova.UI.csproj"
$uiExecutable = Join-Path $packageRoot "Librova.UI.exe"
$hostExecutable = Join-Path $nativeOutputRoot "LibrovaCoreHostApp.exe"
$packagedHostExecutable = Join-Path $packageRoot "LibrovaCoreHostApp.exe"

function Assert-LastExitCode([string]$stepName) {
    if ($LASTEXITCODE -ne 0) {
        throw "$stepName failed with exit code $LASTEXITCODE."
    }
}

function Reset-Directory([string]$path) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $path | Out-Null
}

function Copy-NativeArtifacts([string]$sourceDirectory, [string]$destinationDirectory) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory "LibrovaCoreHostApp.exe") -Destination $destinationDirectory -Force

    Get-ChildItem -LiteralPath $sourceDirectory -File |
        Where-Object { $_.Extension -in ".dll", ".json" } |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $destinationDirectory -Force
        }
}

function Invoke-SmokeTest([string]$uiPath, [string]$packageDirectory) {
    $runtimeRoot = Join-Path $packageDirectory "runtime-smoke"
    $uiStateFile = Join-Path $runtimeRoot "ui-shell-state.json"
    $uiPreferencesFile = Join-Path $runtimeRoot "ui-preferences.json"
    $uiLogFile = Join-Path $runtimeRoot "bootstrap-ui.log"

    Reset-Directory $runtimeRoot

    $originalUiState = $env:LIBROVA_UI_STATE_FILE
    $originalUiPreferences = $env:LIBROVA_UI_PREFERENCES_FILE
    $originalUiLog = $env:LIBROVA_UI_LOG_FILE
    $originalLibraryRoot = $env:LIBROVA_LIBRARY_ROOT
    $originalHostOverride = $env:LIBROVA_CORE_HOST_EXECUTABLE

    try {
        $env:LIBROVA_UI_STATE_FILE = $uiStateFile
        $env:LIBROVA_UI_PREFERENCES_FILE = $uiPreferencesFile
        $env:LIBROVA_UI_LOG_FILE = $uiLogFile
        Remove-Item Env:LIBROVA_LIBRARY_ROOT -ErrorAction SilentlyContinue
        Remove-Item Env:LIBROVA_CORE_HOST_EXECUTABLE -ErrorAction SilentlyContinue

        $process = Start-Process -FilePath $uiPath -WorkingDirectory $packageDirectory -PassThru

        try {
            Start-Sleep -Seconds 5
            if ($process.HasExited) {
                throw "Packaged UI exited during smoke test with code $($process.ExitCode)."
            }
        }
        finally {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
        }
    }
    finally {
        if ($null -ne $originalUiState) {
            $env:LIBROVA_UI_STATE_FILE = $originalUiState
        }
        else {
            Remove-Item Env:LIBROVA_UI_STATE_FILE -ErrorAction SilentlyContinue
        }

        if ($null -ne $originalUiPreferences) {
            $env:LIBROVA_UI_PREFERENCES_FILE = $originalUiPreferences
        }
        else {
            Remove-Item Env:LIBROVA_UI_PREFERENCES_FILE -ErrorAction SilentlyContinue
        }

        if ($null -ne $originalUiLog) {
            $env:LIBROVA_UI_LOG_FILE = $originalUiLog
        }
        else {
            Remove-Item Env:LIBROVA_UI_LOG_FILE -ErrorAction SilentlyContinue
        }

        if ($null -ne $originalLibraryRoot) {
            $env:LIBROVA_LIBRARY_ROOT = $originalLibraryRoot
        }
        else {
            Remove-Item Env:LIBROVA_LIBRARY_ROOT -ErrorAction SilentlyContinue
        }

        if ($null -ne $originalHostOverride) {
            $env:LIBROVA_CORE_HOST_EXECUTABLE = $originalHostOverride
        }
        else {
            Remove-Item Env:LIBROVA_CORE_HOST_EXECUTABLE -ErrorAction SilentlyContinue
        }

        if (Test-Path -LiteralPath $runtimeRoot) {
            Remove-Item -LiteralPath $runtimeRoot -Recurse -Force
        }
    }
}

Push-Location $repoRoot
try {
    Write-Host "==> Preparing portable package layout"
    Reset-Directory $packageRoot
    Reset-Directory $publishRoot

    if (-not $SkipConfigure) {
        Write-Host "==> Configuring native release preset ($NativePreset)"
        cmake --preset $NativePreset
        Assert-LastExitCode "CMake configure"
    }

    Write-Host "==> Building native host ($Configuration)"
    cmake --build --preset $NativePreset --config $Configuration --target LibrovaCoreHostApp
    Assert-LastExitCode "Native host build"

    if (-not (Test-Path -LiteralPath $hostExecutable)) {
        throw "Native host executable was not found: $hostExecutable"
    }

    Write-Host "==> Publishing UI ($Configuration, $RuntimeIdentifier)"
    dotnet publish $uiProject `
        -c $Configuration `
        -r $RuntimeIdentifier `
        --self-contained true `
        /p:PublishSingleFile=true `
        /p:DebugType=None `
        /p:DebugSymbols=false `
        -o $publishRoot
    Assert-LastExitCode "UI publish"

    Write-Host "==> Assembling portable package"
    Get-ChildItem -LiteralPath $publishRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Recurse -Force
    }
    Copy-NativeArtifacts $nativeOutputRoot $packageRoot

    if (-not (Test-Path -LiteralPath $uiExecutable)) {
        throw "Packaged UI executable was not found: $uiExecutable"
    }

    if (-not (Test-Path -LiteralPath $packagedHostExecutable)) {
        throw "Packaged native host executable was not found: $packagedHostExecutable"
    }

    if (-not $SkipSmokeTest) {
        Write-Host "==> Running portable smoke test"
        Invoke-SmokeTest $uiExecutable $packageRoot
    }

    Write-Host "==> Portable package is ready"
    Write-Host "    Package root: $packageRoot"
    Write-Host "    UI exe:       $uiExecutable"
    Write-Host "    Host exe:     $packagedHostExecutable"
}
finally {
    Pop-Location
}
