#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"
BIN_DIR="$ROOT_DIR/bin"

mkdir -p "$BIN_DIR"

shopt -s nullglob
sources=("$SRC_DIR"/*.c)

if [ "${#sources[@]}" -eq 0 ]; then
  echo "No C source files found in $SRC_DIR. Nothing to build."
  exit 0
fi

CC_BIN="${CC:-cc}"

# Default flags. If CFLAGS is set in the environment, split it into words.
if [ -n "${CFLAGS:-}" ]; then
  # shellcheck disable=SC2206
  CFLAGS_ARR=($CFLAGS)
else
  CFLAGS_ARR=(-O2 -Wall -Wextra -std=c11)
fi

for src in "${sources[@]}"; do
  name="$(basename "${src%.c}")"
  out="$BIN_DIR/$name"

  echo "Building $(basename "$src") -> $out"

  "$CC_BIN" "${CFLAGS_ARR[@]}" "$src" -o "$out"
done

echo "Build complete. Binaries are in $BIN_DIR"
