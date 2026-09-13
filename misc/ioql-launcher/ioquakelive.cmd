@echo off
rem ioquakelive: comprueba actualizaciones y abre el juego.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" %*
if errorlevel 1 pause
