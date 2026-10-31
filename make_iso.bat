@echo off
setlocal
title Zarqaa OS - ISO Maker
rem One click: smart clean + build + verify ISO, then optionally burn it to USB.
rem   make_iso.bat          incremental build
rem   make_iso.bat clean    force a full rebuild
cd /d "%~dp0"
set "ZARQAA_DIR=%~dp0"

rem Find MSYS2 bash (override with the MSYS_BASH environment variable)
if defined MSYS_BASH if exist "%MSYS_BASH%" goto :have_bash
for %%B in (
    "C:\msys64\usr\bin\bash.exe"
    "%USERPROFILE%\msys64\usr\bin\bash.exe"
    "%USERPROFILE%\Downloads\msys64\usr\bin\bash.exe"
    "C:\Users\ipiou\Downloads\msys64\usr\bin\bash.exe"
) do if exist %%B (
    set "MSYS_BASH=%%~B"
    goto :have_bash
)
echo [ERROR] MSYS2 was not found. Install it or set MSYS_BASH to its usr\bin\bash.exe
pause
exit /b 1

:have_bash
"%MSYS_BASH%" -l -c "cd \"$(cygpath -u \"$ZARQAA_DIR\")\" && ./tools/build_iso.sh %*"
if errorlevel 1 (
    echo.
    echo [ERROR] The ISO was not built. See the messages above.
    pause
    exit /b 1
)

echo.
choice /C YN /M "Burn the ISO to a USB stick now"
if errorlevel 2 (
    echo The ISO is in the dist folder.
    pause
    exit /b 0
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\burn_usb.ps1"
