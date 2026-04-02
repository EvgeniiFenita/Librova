param(
    [string]$OutputPath = "apps\Librova.UI\Assets\librova.ico"
)

$ErrorActionPreference = "Stop"
python (Join-Path $PSScriptRoot "GenerateLibrovaIcon.py") $OutputPath
