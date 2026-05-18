$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BinDir = Join-Path $Root "bin"
$LogDir = Join-Path $Root "logs"
$DiagLogDir = Join-Path $LogDir "diagnostic"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
New-Item -ItemType Directory -Force -Path $DiagLogDir | Out-Null

# Word certificate files.
# These paths assume the files are in the repository root.
$S = Join-Path $Root "s_certificate.txt"
$T = Join-Path $Root "t_certificate.txt"
$U = Join-Path $Root "u_certificate.txt"
$V = Join-Path $Root "v_certificate.txt"

# If your word certificates are under certificates\, replace the four lines above by:
# $S = Join-Path $Root "certificates\s_certificate.txt"
# $T = Join-Path $Root "certificates\t_certificate.txt"
# $U = Join-Path $Root "certificates\u_certificate.txt"
# $V = Join-Path $Root "certificates\v_certificate.txt"

$MainCert = Join-Path $Root "certificates\certificate.txt"

function Require-File($Path) {
    if (-not (Test-Path $Path)) {
        throw "Required file not found: $Path"
    }
}

function Require-Bin($Name) {
    $Path = Join-Path $BinDir $Name
    if (-not (Test-Path $Path)) {
        throw "Required binary not found: $Path"
    }
    return $Path
}

Write-Host "Checking required certificate files..."
Require-File $S
Require-File $T
Require-File $U
Require-File $V
Require-File $MainCert

Write-Host "Checking required binaries..."

$TraceGen = Require-Bin "trace_generator.exe"
$CComp    = Require-Bin "cycle_composition_checker.exe"
$CFactor  = Require-Bin "cycle_composition_factor_checker.exe"
$CAnchor  = Require-Bin "coverage_anchor_checker.exe"
$WCheck   = Require-Bin "well_definedness_induction_checker_v2.exe"
$Faith    = Require-Bin "faithfulness_checker.exe"

$CPattern = Join-Path $BinDir "cycle_composition_pattern_checker.exe"

Write-Host "Running trace generator..."
cmd /c "`"$TraceGen`" 50000000 > `"$LogDir\trace_generation.log`" 2>&1"

Write-Host "Running cycle composition checker..."
cmd /c "`"$CComp`" `"$S`" S `"$T`" T `"$U`" U `"$V`" V 28 53 > `"$LogDir\cycle_composition_checker.log`" 2>&1"

Write-Host "Running cycle composition factor checker..."
cmd /c "`"$CFactor`" `"$S`" S `"$T`" T `"$U`" U `"$V`" V 28 > `"$LogDir\cycle_composition_factor_checker.log`" 2>&1"

Write-Host "Running coverage anchor checker..."
cmd /c "`"$CAnchor`" `"$S`" `"$T`" `"$U`" `"$V`" 28 > `"$LogDir\coverage_anchor_checker.log`" 2>&1"

Write-Host "Running well-definedness induction checker..."
cmd /c "`"$WCheck`" `"$MainCert`" > `"$LogDir\well_definedness_induction_checker.log`" 2>&1"

Write-Host "Running faithfulness checker..."
cmd /c "`"$Faith`" `"$MainCert`" > `"$LogDir\faithfulness_checker.log`" 2>&1"

if (Test-Path $CPattern) {
    Write-Host "Running diagnostic pattern checker..."
    cmd /c "`"$CPattern`" `"$S`" S `"$T`" T `"$U`" U `"$V`" V 28 53 > `"$DiagLogDir\cycle_composition_pattern_checker.log`" 2>&1"
} else {
    Write-Host "Diagnostic pattern checker not found; skipping."
}

Write-Host ""
Write-Host "All checks finished. Logs are in:"
Write-Host $LogDir
