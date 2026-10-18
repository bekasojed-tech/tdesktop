@echo off
setlocal
set "TELEGRAM_EXE=%~dp0out\Debug\Telegram.exe"
set "TELEGRAM_PROFILE=%~dp0out\local-profile"
if not exist "%TELEGRAM_EXE%" (
    echo Debug build not found: "%TELEGRAM_EXE%"
    echo See docs\local-windows-build.md for setup and build instructions.
    pause
    exit /b 1
)
if not exist "%TELEGRAM_PROFILE%\" mkdir "%TELEGRAM_PROFILE%"
if not exist "%TELEGRAM_PROFILE%\" (
    echo Could not create profile directory: "%TELEGRAM_PROFILE%"
    pause
    exit /b 1
)
start "Telegram Debug" /D "%~dp0out\Debug" "%TELEGRAM_EXE%" -workdir "%TELEGRAM_PROFILE%" -noupdate
exit /b %errorlevel%
