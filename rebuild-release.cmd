@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Telegram\build\rebuild_debug.ps1" -Configuration Release %*
exit /b %errorlevel%
