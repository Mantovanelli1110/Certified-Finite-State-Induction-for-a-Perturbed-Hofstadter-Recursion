$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$LogDir = Join-Path $Root "logs"
$BinDir = Join-Path $Root "bin"
$CertDir = Join-Path $Root "certificates"
$TimeStamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$RunLog = Join-Path $LogDir ("run_all_checks_" + $TimeStamp + ".log")

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

& (Join-Path $PSScriptRoot "build_windows.ps1") | Tee-Object -FilePath $RunLog

$Bins = Get-ChildItem -Path $BinDir -File -ErrorAction SilentlyContinue
if (-not $Bins) {
    "No binaries found in $BinDir. Exiting." | Tee-Object -FilePath $RunLog -Append
    exit 0
}

$Certs = Get-ChildItem -Path $CertDir -File -ErrorAction SilentlyContinue
if (-not $Certs) {
    "No certificate files found in $CertDir. Running checkers without certificate arguments." | Tee-Object -FilePath $RunLog -Append
    foreach ($Bin in $Bins) {
        "=== Running $($Bin.Name) ===" | Tee-Object -FilePath $RunLog -Append
        & $Bin.FullName 2>&1 | Tee-Object -FilePath $RunLog -Append
    }
    exit 0
}

foreach ($Bin in $Bins) {
    foreach ($Cert in $Certs) {
        "=== Running $($Bin.Name) on $($Cert.Name) ===" | Tee-Object -FilePath $RunLog -Append
        & $Bin.FullName $Cert.FullName 2>&1 | Tee-Object -FilePath $RunLog -Append
    }
}

"Run complete: $RunLog" | Tee-Object -FilePath $RunLog -Append
