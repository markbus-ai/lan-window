#!/usr/bin/env bash
# Arma una carpeta portable del RECEPTOR con Qt, GStreamer y sus DLL. Ejecutar en "MSYS2 UCRT64".
# Uso: scripts/package-windows-msys2.sh <directorio-de-compilación> <carpeta-de-salida>
set -euo pipefail
if [[ "${MSYSTEM:-}" != "UCRT64" || $# -ne 2 ]]; then
  echo "Uso (consola MSYS2 UCRT64): $0 <directorio-de-compilación> <carpeta-de-salida>" >&2
  exit 1
fi
BUILD="$1"
OUT="$2"
PREFIX=/ucrt64
GST_PLUGINS="$PREFIX/lib/gstreamer-1.0"
QT_PLUGINS="$PREFIX/share/qt6/plugins"

# Copia a OUT las DLL de MSYS2 de las que depende un binario, de forma recursiva.
copy_dependencies() {
  ntldd -R "$1" | tr -d '\r' | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while IFS= read -r dll; do
    path="$(cygpath -u "$dll")"
    if [[ "$path" == "$PREFIX/bin/"* ]]; then
      cp -n "$path" "$OUT/"
    fi
  done
}

rm -rf "$OUT"
mkdir -p "$OUT/platforms" "$OUT/styles" "$OUT/gstreamer-1.0"
cp "$BUILD/lan-window-receiver.exe" "$OUT/"
cp "$QT_PLUGINS/platforms/qwindows.dll" "$OUT/platforms/"
cp "$QT_PLUGINS/styles/"*.dll "$OUT/styles/"

# Plugins de los elementos en requiredReceiverElements(); --self-check comprueba que basten.
for plugin in coreelements app udp rtp rtpmanager videoparsersbad libav videoconvertscale; do
  cp "$GST_PLUGINS/libgst$plugin.dll" "$OUT/gstreamer-1.0/"
done

copy_dependencies "$OUT/lan-window-receiver.exe"
for binary in "$OUT"/platforms/*.dll "$OUT"/styles/*.dll "$OUT"/gstreamer-1.0/*.dll; do
  copy_dependencies "$binary"
done

echo "Paquete: $OUT ($(du -sh "$OUT" | cut -f1))"
