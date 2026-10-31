@echo off
title Zarqaa OS - Auto Build & Run
rem Build the folder this script lives in, not a hard-coded copy
cd /d "%~dp0"
set "ZARQAA_DIR=%~dp0"
if not defined MSYS_BASH set "MSYS_BASH=C:\Users\ipiou\Downloads\msys64\usr\bin\bash.exe"

echo ===================================================
echo [1/2] Compiling Zarqaa OS & Building ISO...
echo ===================================================
"%MSYS_BASH%" -l -c "cd \"$(cygpath -u \"$ZARQAA_DIR\")\" && make all"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed! Check compiler messages above.
    pause
    exit /b %ERRORLEVEL%
)

echo ===================================================
echo [2/2] Launching Zarqaa OS in QEMU (SMP 2, Multi-Threaded TCG)...
echo ===================================================
"C:\Program Files\qemu\qemu-system-x86_64.exe" -accel tcg,thread=multi -smp 2 -cpu max -m 2G -M q35 -bios ovmf/OVMF.fd -device virtio-vga -cdrom build/zarqaaos.iso -drive file=fat:rw:fat_root,format=raw,media=disk,if=ide,index=0 -no-reboot -no-shutdown -serial stdio -netdev user,id=u1 -device rtl8139,netdev=u1 -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0
