#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"
BIN_DIR="$ROOT_DIR/bin"

mkdir -p "$BIN_DIR"

shopt -s nullglob
sources=("$SRC_DIR"/*.c)

if [ ${#sources[@]} -eq 0 ]; then
  echo "No C source files found in $SRC_DIR. Nothing to build."
  exit 0
fi

CC_BIN="${CC:-cc}"
CFLAGS_BIN="${CFLAGS:--O2 -Wall -Wextra -std=c11}"

for src in "${sources[@]}"; do
  name="$(basename "${src%.c}")"
  echo "Building $name from $(basename "$src")"
  "$CC_BIN" $CFLAGS_BIN "$src" -o "$BIN_DIR/$name"
done

echo "Build complete. Binaries are in $BIN_DIR"
