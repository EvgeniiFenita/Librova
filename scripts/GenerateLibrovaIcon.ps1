param(
    [string]$OutputPath = "apps\Librova.UI\Assets\librova.ico"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $resolvedOutputPath = $OutputPath
}
else {
    $resolvedOutputPath = Join-Path $repoRoot $OutputPath
}

python (Join-Path $PSScriptRoot "GenerateLibrovaIcon.py") $resolvedOutputPath
