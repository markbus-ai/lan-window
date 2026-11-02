@echo off
rem Lanza el receptor usando las DLL de Qt/GStreamer de MSYS2 UCRT64.
rem Uso: run-receiver-windows.cmd [opciones del receptor, p. ej. --port 5004 --autostart]
rem Variables opcionales: MSYS2_ROOT (por defecto C:\msys64),
rem                       LANWINDOW_EXE (por defecto ..\..\lan-window-build\lan-window-receiver.exe)
setlocal
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%PATH%"
if "%LANWINDOW_EXE%"=="" set "LANWINDOW_EXE=%~dp0..\..\lan-window-build\lan-window-receiver.exe"
if not exist "%LANWINDOW_EXE%" (
  echo No se encuentra "%LANWINDOW_EXE%". Define LANWINDOW_EXE con la ruta del .exe.
  exit /b 1
)
start "" "%LANWINDOW_EXE%" %*
