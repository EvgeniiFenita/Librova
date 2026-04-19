param(
    [string]$Preset = "x64-debug",
    [string]$Configuration = "Debug",
    [switch]$SkipConfigure,
    [switch]$SkipNative,
    [switch]$SkipManaged,
    [int]$ParallelJobs = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS)
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

        if (-not $SkipManaged) {
            # Build managed suite now so both test runs can start at the same time.
            if (Test-Path -LiteralPath $hostExecutable) {
                $env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExecutable
            }

            Write-Host "==> Building managed UI tests ($Configuration)"
            dotnet build $uiTestsProject -c $Configuration --nologo --verbosity quiet -tl:off
            Assert-LastExitCode "Managed build"
        }

        # Capture env snapshot for the background job (Start-Job runs in a child runspace).
        $hostExe = $env:LIBROVA_CORE_HOST_EXECUTABLE

        Write-Host "==> Running native tests ($Configuration, -j $ParallelJobs)"
        $ctestJob = Start-Job -ScriptBlock {
            param($testDir, $config, $j)
            $output = & ctest --test-dir $testDir -C $config --output-on-failure -j $j 2>&1
            [PSCustomObject]@{ Lines = [string[]]$output; ExitCode = $LASTEXITCODE }
        } -ArgumentList $nativePresetRoot, $Configuration, $ParallelJobs

        if (-not $SkipManaged) {
            Write-Host "==> Running managed UI tests ($Configuration)"
            $env:LIBROVA_CORE_HOST_EXECUTABLE = $hostExe
            dotnet test $uiTestsProject --no-build -c $Configuration --nologo -tl:off -- --no-progress
            $managedExitCode = $LASTEXITCODE
        }

        $ctestResult = Receive-Job -Job $ctestJob -Wait -AutoRemoveJob
        $ctestResult.Lines | Where-Object {
            ($_ -notmatch '^\s*Start\s+\d+:') -and
            ($_ -notmatch '^\s*\d+/\d+\s+Test\s+#\d+:.*\bPassed\b')
        } | ForEach-Object { Write-Host $_ }

        if ($ctestResult.ExitCode -ne 0) {
            throw "Native tests failed with exit code $($ctestResult.ExitCode)."
        }

        if (-not $SkipManaged -and $managedExitCode -ne 0) {
            throw "Managed tests failed with exit code $managedExitCode."
        }
    } elseif (-not $SkipManaged) {
        # Only managed suite requested (native skipped).
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
