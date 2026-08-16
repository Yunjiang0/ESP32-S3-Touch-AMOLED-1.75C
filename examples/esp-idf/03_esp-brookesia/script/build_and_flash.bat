@echo off
REM ============================================================================
REM ESP-Brookesia + Wallpaper App 一键打包烧录脚本
REM
REM Usage:
REM   build_and_flash.bat              - auto-detect COM port
REM   build_and_flash.bat COM3         - use COM3
REM   build_and_flash.bat COM5 build   - only build, skip flash
REM   build_and_flash.bat COM3 monitor - build, flash, then monitor
REM
REM Toolchain expected at:
REM   D:\esp\esp-idf          - ESP-IDF v5.5.5
REM   D:\esp\.espressif       - IDF tools (xtensa-elf, cmake, python venv)
REM ============================================================================

setlocal

REM ---- locate project directory (this script's parent) ----
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"
echo [setup] project dir: %PROJECT_DIR%

REM ---- pick up COM port from arg or auto-detect ----
set "PORT=%~1"
set "MODE=%~2"
if "%MODE%"=="" set "MODE=flash"
if "%PORT%"=="" (
    echo [auto-detect] scanning for Espressif USB serial ports ...
    powershell.exe -NoProfile -Command ^
        "$ports = Get-WmiObject Win32_SerialPort ^| Select-Object -ExpandProperty DeviceID;" ^
        "if ($ports) { Write-Output ('Detected ports: ' + ($ports -join ', ')) }" 2>nul
    echo Please re-run with the COM port as the first argument, e.g.:
    echo     %~nx0 COM3
    echo     %~nx0 COM5
    pause
    exit /b 1
)
echo [setup] using %PORT%

REM ---- ESP-IDF environment ----
set MSYSTEM=
set ESP_IDF_VERSION=v5.5.5

set "IDF_PATH=D:\esp\esp-idf"
set "IDF_TOOLS_PATH=D:\esp\.espressif"

set "ESP_ROM_ELF_DIR=D:\esp\.espressif\tools\esp-rom-elfs\20241011"

set "PYTHON_EXE=D:\esp\.espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe"

REM Prepend toolchain to PATH (v5.5.5 toolchain versions)
set "PATH=D:\esp\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;D:\esp\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;D:\esp\.espressif\tools\cmake\3.30.2\bin;D:\esp\.espressif\tools\ninja\1.12.1;D:\esp\.espressif\tools\openocd-esp32\v0.12.0-esp32-20241016\openocd-esp32\bin;D:\esp\.espressif\python_env\idf5.5_py3.13_env\Scripts;%PATH%"

set "ESPTOOL_PY=D:\esp\esp-idf\components\esptool_py\esptool\esptool.py"

REM ---- ensure target is esp32s3 ----
if not exist "sdkconfig" (
    echo [setup] sdkconfig not found, running set-target esp32s3 ...
    "%PYTHON_EXE%" "%IDF_PATH%\tools\idf.py" set-target esp32s3
    if errorlevel 1 (
        echo [setup] set-target FAILED
        pause
        exit /b 1
    )
) else (
    REM Check if current target is esp32s3
    findstr /C:"CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig >nul 2>&1
    if errorlevel 1 (
        echo [setup] wrong target detected, switching to esp32s3 ...
        if exist build rd /s /q build
        "%PYTHON_EXE%" "%IDF_PATH%\tools\idf.py" set-target esp32s3
        if errorlevel 1 (
            echo [setup] set-target FAILED
            pause
            exit /b 1
        )
    )
)

REM ---- build ----
echo.
echo === BUILD =====================================================
"%PYTHON_EXE%" "%IDF_PATH%\tools\idf.py" build
if errorlevel 1 (
    echo [build] FAILED
    pause
    exit /b 1
)
echo [build] OK

if /i "%MODE%"=="build" goto :skip_flash

:flash
echo.
echo === FLASH =====================================================
REM Kill any stuck monitor / serial tool that might hold the port.
taskkill /F /IM python.exe 2>nul
timeout /t 2 /nobreak >nul

REM Invoke esptool.py directly
cd /d "%PROJECT_DIR%\build"
"%PYTHON_EXE%" "%ESPTOOL_PY%" ^
    --chip esp32s3 ^
    -p %PORT% ^
    -b 460800 ^
    --before default_reset ^
    --after hard_reset ^
    write_flash @flash_args
if errorlevel 1 (
    echo [flash] FAILED
    pause
    exit /b 1
)
echo [flash] OK

REM Leave download mode and boot the app
echo [run] leaving download mode ...
"%PYTHON_EXE%" "%ESPTOOL_PY%" ^
    --chip esp32s3 ^
    -p %PORT% ^
    run
if errorlevel 1 (
    echo [run] FAILED
    pause
    exit /b 1
)

REM Pulse DTR for normal boot (Waveshare auto-reset circuit fix)
echo [reset] pulsing DTR for normal boot ...
"%PYTHON_EXE%" -c "import serial, time; sp=serial.Serial('%PORT%', 115200); sp.setDTR(False); sp.setRTS(False); time.sleep(0.1); sp.setDTR(True); time.sleep(0.1); sp.setDTR(False); sp.close(); print('reset ok')" 2>nul

if /i "%MODE%"=="flash" goto :end

REM ---- monitor ----
:monitor
echo.
echo === MONITOR ==================================================
echo Press Ctrl+] to exit.
cd /d "%PROJECT_DIR%"
"%PYTHON_EXE%" "%IDF_PATH%\tools\idf.py" -p %PORT% monitor
endlocal
goto :eof

:skip_flash
echo.
echo === BUILD ONLY - done ==========================================
pause
endlocal
goto :eof

:end
echo.
echo === ALL DONE ==================================================
echo.
echo If the screen is still black:
echo   1. UNPLUG the USB cable from the board
echo   2. Wait 5 seconds
echo   3. PLUG it back in
echo   4. The app will boot from flash
echo.
pause
endlocal
