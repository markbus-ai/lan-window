# LAN Window

Envía la pantalla de un PC con Linux a un PC con Windows 11 por la red local y la muestra en una ventana que puedes compartir en Discord. Solo vídeo, sin audio.

```text
PC Linux (emisor)                                  PC Windows 11 (receptor)
PipeWire / portal XDG -> H.264 (GStreamer) -> RTP/UDP -> decodificación -> ventana Qt6 -> Discord
```

La integración con Discord consiste en elegir la ventana del receptor al compartir pantalla. La aplicación no automatiza Discord.

## Estado de las pruebas

| Área | Estado |
| --- | --- |
| Compilación en Linux | Probada |
| Transmisión RTP/UDP real por loopback con patrón de prueba (`loopback_test`) | Probada |
| Ventanas de emisor y receptor en modo offscreen (`ui_smoke_test`) | Probada |
| Captura real con el portal de Wayland en Hyprland, por loopback hacia un receptor `gst-launch` | Probada a 720p (unos 28 fps decodificados); no se inspeccionó el contenido de la imagen |
| Paquetes de GitHub Actions (receptor de Windows y AppImage del emisor) | **Sin verificar** |
| Compilación y ejecución en Windows | **Sin verificar** |
| Compartir la ventana en Discord | **Sin verificar** |

## Paquetes listos para usar

Cada envío a `main` ejecuta el workflow `build` de GitHub Actions, que publica dos artefactos en la página de la ejecución:

| Artefacto | Dónde se usa | Cómo se inicia |
| --- | --- | --- |
| `lan-window-receiver-windows-x64` | PC con Windows 10 u 11 | Descomprime la carpeta completa y abre `lan-window-receiver.exe` |
| `lan-window-sender-linux-x86_64` | PC con Linux y PipeWire | Descomprime, ejecuta `chmod +x lan-window-sender-x86_64.AppImage` y ábrelo |

Requisitos y avisos:

- El ejecutable de Windows no está firmado, así que SmartScreen puede advertir al abrirlo.
- El AppImage necesita FUSE 2, una sesión con portal XDG de captura de pantalla y una glibc igual o más reciente que la de Ubuntu 24.04.
- Ambos PC deben estar en la misma red local.

## Linux: compilar el emisor

Dependencias en Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake pkgconf qt6-base libportal gstreamer \
  gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugin-pipewire gst-libav
```

Compila y ejecuta los tests:

```bash
scripts/build-linux.sh
```

Los binarios quedan en `../lan-window-build/`, o en el directorio que pases como argumento. Se usa `x264enc` si está disponible y, si no, `openh264enc`.

## Windows 11: compilar el receptor

1. Instala [MSYS2](https://www.msys2.org/) en `C:\msys64`.
2. Abre la consola **MSYS2 UCRT64**, ve a la carpeta del proyecto y ejecuta:

   ```bash
   scripts/build-windows-msys2.sh --install-deps
   ```

3. Inicia el receptor desde esa consola o con `scripts\run-receiver-windows.cmd`.

Este ejecutable depende de las DLL de MSYS2. Para una carpeta portable, usa `scripts/package-windows-msys2.sh` o el artefacto de GitHub Actions. Si el ejecutable está en otra ruta, define `MSYS2_ROOT` o `LANWINDOW_EXE` antes de usar el `.cmd`.

## Uso

1. **Windows:** inicia el receptor y pulsa **Iniciar**. Permite el tráfico UDP entrante en el puerto 5004 si el firewall lo solicita.
2. **Linux:** inicia el emisor con la IPv4 del PC con Windows y elige el monitor en el diálogo del sistema:

   ```bash
   ../lan-window-build/lan-window-sender --host 192.168.1.50 --autostart
   ```

3. **Discord:** comparte la ventana **LAN Window – Receptor**. Pulsa `F11` para ocultar o mostrar los controles y `Esc` para volver a mostrarlos. No minimices la ventana mientras la compartes.

Para probar la red sin capturar la pantalla, añade `--test-pattern` al emisor.

## Opciones

| Emisor | Descripción |
| --- | --- |
| `--host <ipv4>` | IPv4 del PC receptor |
| `--port <puerto>` | Puerto UDP de destino (predeterminado: 5004) |
| `--quality 720\|1080` | 720p a 4 Mbit/s o 1080p a 8 Mbit/s, ambos a 30 fps |
| `--test-pattern` | Envía un patrón de prueba |
| `--autostart` | Empieza a enviar al abrir |
| `--self-check` | Comprueba plugins y codificador, y sale (0 = correcto) |

| Receptor | Descripción |
| --- | --- |
| `--bind <ipv4>` | IPv4 local de escucha (predeterminado: `0.0.0.0`) |
| `--port <puerto>` | Puerto UDP (predeterminado: 5004) |
| `--autostart` | Empieza a recibir al abrir |
| `--hide-controls` | Abre con los controles ocultos |
| `--self-check` | Comprueba los plugins necesarios y sale (0 = correcto) |

## Seguridad

El vídeo viaja sin cifrar ni autenticar. Úsalo solo en una red local de confianza.

## Problemas frecuentes

- **El receptor muestra «Sin señal»:** comprueba la IPv4 de destino, el puerto y el firewall de Windows. Prueba con `--test-pattern` para descartar la captura.
- **Faltan plugins de GStreamer:** la ventana indica cuáles; instala el paquete correspondiente.
