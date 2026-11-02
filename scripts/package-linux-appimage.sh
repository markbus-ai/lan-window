#!/usr/bin/env bash
# Arma un AppImage del EMISOR con Qt y GStreamer. Pensado para Ubuntu 24.04 (ver el workflow de CI).
# libpipewire no se incluye: se usa la del sistema, para que coincida con su daemon y sus módulos.
# Uso: scripts/package-linux-appimage.sh <directorio-de-compilación> <carpeta-de-salida>
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "Uso: $0 <directorio-de-compilación> <carpeta-de-salida>" >&2
  exit 1
fi
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(realpath "$1")"
mkdir -p "$2"
OUT="$(realpath "$2")"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/AppDir"
RELEASES=https://github.com/linuxdeploy

curl -fsSL -o "$WORK/linuxdeploy" "$RELEASES/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
curl -fsSL -o "$WORK/linuxdeploy-plugin-qt" "$RELEASES/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x "$WORK/linuxdeploy" "$WORK/linuxdeploy-plugin-qt"

GST_PLUGINS="$(pkg-config --variable=pluginsdir gstreamer-1.0)"
GST_SCANNER="$(pkg-config --variable=pluginscannerdir gstreamer-1.0)/gst-plugin-scanner"

# Plugins de los elementos en requiredSenderElements(true) y x264enc; --self-check comprueba que basten.
mkdir -p "$APPDIR/usr/lib/gstreamer-1.0"
for plugin in coreelements videotestsrc videoconvertscale videoparsersbad rtp udp x264 pipewire; do
  cp "$GST_PLUGINS/libgst$plugin.so" "$APPDIR/usr/lib/gstreamer-1.0/"
done

export PATH="$WORK:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/lib/qt6/bin/qmake6
export EXTRA_PLATFORM_PLUGINS=libqwayland-generic.so
export EXTRA_QT_MODULES=waylandclient
export OUTPUT="$OUT/lan-window-sender-x86_64.AppImage"

"$WORK/linuxdeploy" --appdir "$APPDIR" \
  --executable "$BUILD/lan-window-sender" \
  --executable "$GST_SCANNER" \
  --deploy-deps-only "$APPDIR/usr/lib/gstreamer-1.0" \
  --exclude-library 'libpipewire-0.3*' \
  --desktop-file "$SRC/packaging/lan-window-sender.desktop" \
  --icon-file "$SRC/packaging/lan-window-sender.svg" \
  --custom-apprun "$SRC/packaging/AppRun" \
  --plugin qt \
  --output appimage

echo "Paquete: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
