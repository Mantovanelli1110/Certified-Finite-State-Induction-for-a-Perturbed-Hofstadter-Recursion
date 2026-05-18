$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Src = Join-Path $Root "src"
$Bin = Join-Path $Root "bin"

New-Item -ItemType Directory -Force -Path $Bin | Out-Null

$Sources = Get-ChildItem -Path $Src -Filter *.c -File -ErrorAction SilentlyContinue
if (-not $Sources) {
    Write-Host "No C source files found in $Src. Nothing to build."
    exit 0
}

$CC = if ($env:CC) { $env:CC } else { "gcc" }

if ($env:CFLAGS) {
    $CFlags = $env:CFLAGS -split '\s+'
} else {
    $CFlags = @("-O2", "-Wall", "-Wextra", "-std=c11")
}

foreach ($File in $Sources) {
    $Out = Join-Path $Bin ($File.BaseName + ".exe")

    Write-Host "Building $($File.Name) -> $Out"

    & $CC @CFlags $File.FullName -o $Out

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $($File.Name)"
    }
}

Write-Host "Build complete. Binaries are in $Bin"
