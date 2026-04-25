<#
.SYNOPSIS
    Validates that no Qt headers leak into the pure-C++ library code under libs/.
    Run this after any change to libs/ to confirm the C++ / Qt boundary is clean.

.DESCRIPTION
    Scans all .hpp and .cpp files under libs/ for #include directives that reference
    Qt headers (any path containing "/Qt" or starting with "Q").
    Exits with code 1 and prints offending file+line if any violations are found.
#>

$repoRoot = Split-Path -Parent $PSScriptRoot
$libsDir  = Join-Path $repoRoot "libs"

$violations = @()

Get-ChildItem -Path $libsDir -Recurse -Include "*.hpp","*.cpp" |
    ForEach-Object {
        $file = $_
        $lineNumber = 0
        Get-Content $file.FullName | ForEach-Object {
            $lineNumber++
            $line = $_
            if ($line -match '^\s*#\s*include\s+[<"]') {
                # Match Qt system headers: <QtCore/...>, <QObject>, <qobject.h>, etc.
                if ($line -match '[<"](Qt[A-Za-z0-9]+/|Q[A-Z][A-Za-z0-9]*\.h|q[a-z]+\.h)') {
                    $violations += [PSCustomObject]@{
                        File   = $file.FullName
                        Line   = $lineNumber
                        Content = $line.Trim()
                    }
                }
            }
        }
    }

if ($violations.Count -eq 0) {
    Write-Host "ValidateBoundary: OK - no Qt headers found under libs/." -ForegroundColor Green
    exit 0
}

Write-Host "ValidateBoundary: FAILED - Qt headers found in pure-C++ library code:" -ForegroundColor Red
foreach ($v in $violations) {
    Write-Host ("  {0}:{1}  {2}" -f $v.File, $v.Line, $v.Content) -ForegroundColor Red
}
exit 1
