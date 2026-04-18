param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [switch]$SkipConfigure,
    [switch]$SkipNative,
    [switch]$SkipManaged
)

$ErrorActionPreference = "Stop"

$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
$env:DOTNET_NOLOGO = "1"

$repoRoot = Split-Path -Parent $PSScriptRoot
$nativePresetRoot = Join-Path $repoRoot "out\build\$Preset"
$hostExecutable = Join-Path $nativePresetRoot "apps\Librova.Core.Host\$Configuration\LibrovaCoreHostApp.exe"
$uiTestsProject = Join-Path $repoRoot "tests\Librova.UI.Tests\Librova.UI.Tests.csproj"

function Assert-LastExitCode([string]$stepName) {
    if ($LASTEXITCODE -ne 0) {
        throw "$stepName failed with exit code $LASTEXITCODE."
    }
}

Push-Location $repoRoot
try {
    if (-not $SkipNative) {
        if (-not $SkipConfigure) {
            Write-Host "==> Configuring native build ($Preset)"
            & cmake --preset $Preset --log-level=WARNING 2>&1 | Where-Object {
                ($_ -match '^--\s') -or ($_ -match '\b(error|warning)\b')
            }
            $configureExitCode = $LASTEXITCODE
            if ($configureExitCode -ne 0) {
                Write-Host ""
                Write-Host "==> CMake configure failed -- re-running without filter for full diagnostics:"
                cmake --preset $Preset
                throw "CMake configure failed with exit code $configureExitCode."
            }
        }

        Write-Host "==> Building native code ($Configuration)"
        cmake --build --preset $Preset --config $Configuration -- /verbosity:quiet /nologo
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0) {
            Write-Host ""
            Write-Host "==> Native build failed -- re-running without filter for full diagnostics:"
            cmake --build --preset $Preset --config $Configuration
            throw "Native build failed with exit code $buildExitCode."
        }

        Write-Host "==> Running native tests ($Configuration)"
        & ctest --test-dir $nativePresetRoot -C $Configuration --output-on-failure 2>&1 | Where-Object {
            ($_ -notmatch '^\s*Start\s+\d+:') -and
            ($_ -notmatch '^\s*\d+/\d+\s+Test\s+#\d+:.*\bPassed\b')
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Native tests failed with exit code $LASTEXITCODE."
        }
    }

    if (-not $SkipManaged) {
        if (Test-Path -LiteralPath $hostExecutable) {
            $env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExecutable
        }

        Write-Host "==> Building managed UI tests ($Configuration)"
        dotnet build $uiTestsProject -c $Configuration --nologo --verbosity quiet -tl:off
        Assert-LastExitCode "Managed build"

        Write-Host "==> Running managed UI tests ($Configuration)"
        dotnet test $uiTestsProject --no-build -c $Configuration --nologo -tl:off -- --no-progress
        Assert-LastExitCode "Managed tests"
    }

    Write-Host "==> All requested tests completed successfully."
}
finally {
    Pop-Location
}
