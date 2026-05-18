#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
LOG_DIR="$ROOT_DIR/logs"
DIAG_LOG_DIR="$LOG_DIR/diagnostic"

mkdir -p "$LOG_DIR"
mkdir -p "$DIAG_LOG_DIR"

# Word certificate files.
# These paths assume the files are in the repository root.
S_CERT="$ROOT_DIR/s_certificate.txt"
T_CERT="$ROOT_DIR/t_certificate.txt"
U_CERT="$ROOT_DIR/u_certificate.txt"
V_CERT="$ROOT_DIR/v_certificate.txt"

# If your word certificates are under certificates/, replace the four lines above by:
# S_CERT="$ROOT_DIR/certificates/s_certificate.txt"
# T_CERT="$ROOT_DIR/certificates/t_certificate.txt"
# U_CERT="$ROOT_DIR/certificates/u_certificate.txt"
# V_CERT="$ROOT_DIR/certificates/v_certificate.txt"

MAIN_CERT="$ROOT_DIR/certificates/certificate.txt"

require_file() {
  if [ ! -f "$1" ]; then
    echo "Required file not found: $1" >&2
    exit 1
  fi
}

require_bin() {
  if [ ! -x "$1" ]; then
    echo "Required binary not found or not executable: $1" >&2
    exit 1
  fi
}

echo "Building checkers..."
bash "$ROOT_DIR/scripts/build_unix.sh"

echo "Checking required certificate files..."
require_file "$S_CERT"
require_file "$T_CERT"
require_file "$U_CERT"
require_file "$V_CERT"
require_file "$MAIN_CERT"

TRACE_GEN="$BIN_DIR/trace_generator"
C_COMP="$BIN_DIR/cycle_composition_checker"
C_FACTOR="$BIN_DIR/cycle_composition_factor_checker"
C_ANCHOR="$BIN_DIR/coverage_anchor_checker"
W_CHECK="$BIN_DIR/well_definedness_induction_checker_v2"
FAITH="$BIN_DIR/faithfulness_checker"
C_PATTERN="$BIN_DIR/cycle_composition_pattern_checker"

echo "Checking required binaries..."
require_bin "$TRACE_GEN"
require_bin "$C_COMP"
require_bin "$C_FACTOR"
require_bin "$C_ANCHOR"
require_bin "$W_CHECK"
require_bin "$FAITH"

echo "Running trace generator..."
"$TRACE_GEN" 50000000 > "$LOG_DIR/trace_generation.log" 2>&1

echo "Running cycle composition checker..."
"$C_COMP" \
  "$S_CERT" S \
  "$T_CERT" T \
  "$U_CERT" U \
  "$V_CERT" V \
  28 53 > "$LOG_DIR/cycle_composition_checker.log" 2>&1

echo "Running cycle composition factor checker..."
"$C_FACTOR" \
  "$S_CERT" S \
  "$T_CERT" T \
  "$U_CERT" U \
  "$V_CERT" V \
  28 > "$LOG_DIR/cycle_composition_factor_checker.log" 2>&1

echo "Running coverage anchor checker..."
"$C_ANCHOR" \
  "$S_CERT" \
  "$T_CERT" \
  "$U_CERT" \
  "$V_CERT" \
  28 > "$LOG_DIR/coverage_anchor_checker.log" 2>&1

echo "Running well-definedness induction checker..."
"$W_CHECK" "$MAIN_CERT" > "$LOG_DIR/well_definedness_induction_checker.log" 2>&1

echo "Running faithfulness checker..."
"$FAITH" "$MAIN_CERT" > "$LOG_DIR/faithfulness_checker.log" 2>&1

if [ -x "$C_PATTERN" ]; then
  echo "Running diagnostic pattern checker..."
  "$C_PATTERN" \
    "$S_CERT" S \
    "$T_CERT" T \
    "$U_CERT" U \
    "$V_CERT" V \
    28 53 > "$DIAG_LOG_DIR/cycle_composition_pattern_checker.log" 2>&1 || true
else
  echo "Diagnostic pattern checker not found; skipping."
fi

echo
echo "All checks finished. Logs are in:"
echo "$LOG_DIR"
