param(
    [string]$BuildPreset = "x64-debug",
    [string]$ProtoFile = "proto/import_jobs.proto"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$protocPath = Join-Path $repoRoot "out/build/$BuildPreset/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe"

if (-not (Test-Path $protocPath)) {
    throw "protoc.exe was not found at '$protocPath'. Build the preset with protobuf enabled first."
}

$protoDirectory = Join-Path $repoRoot "proto"
$outputDirectory = Join-Path $repoRoot "out/proto"
$protoInputPath = Join-Path $repoRoot $ProtoFile

if (-not (Test-Path $protoInputPath)) {
    throw "Proto file '$protoInputPath' was not found."
}

New-Item -ItemType Directory -Force $outputDirectory | Out-Null

$descriptorFileName = [System.IO.Path]::GetFileNameWithoutExtension($ProtoFile) + ".pb"
$descriptorOutputPath = Join-Path $outputDirectory $descriptorFileName

& $protocPath `
    "--proto_path=$protoDirectory" `
    "--descriptor_set_out=$descriptorOutputPath" `
    $protoInputPath

if ($LASTEXITCODE -ne 0) {
    throw "protoc failed with exit code $LASTEXITCODE."
}

Write-Output "Validated '$ProtoFile' with $protocPath"
Write-Output "Descriptor output: $descriptorOutputPath"
