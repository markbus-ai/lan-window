#!/usr/bin/env bash
# Compila emisor + receptor + tests en Linux y ejecuta los tests.
# Uso: scripts/build-linux.sh [directorio-de-compilación]
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$SRC/../lan-window-build}"
GEN=()
command -v ninja >/dev/null 2>&1 || GEN=(-G "Unix Makefiles")
cmake -S "$SRC" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" -j"$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure
echo
echo "Emisor:   $BUILD/lan-window-sender"
echo "Receptor: $BUILD/lan-window-receiver"
