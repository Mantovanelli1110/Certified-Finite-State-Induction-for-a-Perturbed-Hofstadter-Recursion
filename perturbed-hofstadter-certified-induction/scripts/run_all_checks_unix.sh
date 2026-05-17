#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$ROOT_DIR/logs"
BIN_DIR="$ROOT_DIR/bin"
CERT_DIR="$ROOT_DIR/certificates"
TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_LOG="$LOG_DIR/run_all_checks_$TIMESTAMP.log"

mkdir -p "$LOG_DIR"

bash "$ROOT_DIR/scripts/build_unix.sh" | tee "$RUN_LOG"

shopt -s nullglob
bins=("$BIN_DIR"/*)
certs=("$CERT_DIR"/*)

if [ ${#bins[@]} -eq 0 ]; then
  echo "No binaries found in $BIN_DIR. Exiting." | tee -a "$RUN_LOG"
  exit 0
fi

if [ ${#certs[@]} -eq 0 ]; then
  echo "No certificate files found in $CERT_DIR. Running checkers without certificate arguments." | tee -a "$RUN_LOG"
  for bin in "${bins[@]}"; do
    echo "=== Running $(basename "$bin") ===" | tee -a "$RUN_LOG"
    "$bin" 2>&1 | tee -a "$RUN_LOG" || true
  done
  exit 0
fi

for bin in "${bins[@]}"; do
  for cert in "${certs[@]}"; do
    echo "=== Running $(basename "$bin") on $(basename "$cert") ===" | tee -a "$RUN_LOG"
    "$bin" "$cert" 2>&1 | tee -a "$RUN_LOG" || true
  done
done

echo "Run complete: $RUN_LOG"
