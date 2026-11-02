#!/usr/bin/env bash
# Compila el RECEPTOR en Windows 11 con MSYS2. Ejecutar en la consola "MSYS2 UCRT64".
# Uso: scripts/build-windows-msys2.sh [--install-deps] [directorio-de-compilación]
set -euo pipefail
if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "Abre la consola «MSYS2 UCRT64» (MSYSTEM=UCRT64) y vuelve a ejecutar este script." >&2
  exit 1
fi
if [[ "${1:-}" == "--install-deps" ]]; then
  shift
  # gst-plugins-good: udp, rtp, rtpmanager · bad: h264parse · gst-libav: avdec_h264
  # ugly (x264enc): solo lo necesita el test de loopback.
  pacman -S --needed --noconfirm \
    mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-ntldd mingw-w64-ucrt-x86_64-qt6-base \
    mingw-w64-ucrt-x86_64-gstreamer mingw-w64-ucrt-x86_64-gst-plugins-base \
    mingw-w64-ucrt-x86_64-gst-plugins-good mingw-w64-ucrt-x86_64-gst-plugins-bad \
    mingw-w64-ucrt-x86_64-gst-plugins-ugly mingw-w64-ucrt-x86_64-gst-libav
fi
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$SRC/../lan-window-build}"
cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLANWINDOW_BUILD_SENDER=OFF
cmake --build "$BUILD"
if ! ctest --test-dir "$BUILD" --output-on-failure; then
  [[ -z "${CI:-}" ]] || exit 1
  echo "AVISO: el test de loopback falló (¿falta x264enc/openh264enc?)." >&2
fi
echo
echo "Receptor: $BUILD/lan-window-receiver.exe"
echo "Ejecútalo desde esta consola UCRT64, o con scripts\\run-receiver-windows.cmd"
