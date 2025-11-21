@echo off
setlocal enabledelayedexpansion
REM Build script for Power Limit SIL (Software-in-the-Loop) DLL
REM This compiles all necessary C files into libpl_sil.dll for Python testing

echo ========================================
echo Building Power Limit SIL DLL
echo ========================================

REM Try to find GCC/MinGW in common locations
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found GCC in PATH
    goto :found_gcc
)

echo GCC not found in PATH, searching common locations...

REM Try Anaconda locations (user and system installs)
set "ANACONDA_USER=%USERPROFILE%\anaconda3\Library\mingw-w64\bin\gcc.exe"
if exist "%ANACONDA_USER%" (
    echo Found MinGW in Anaconda (user install)
    set "ANACONDA_PATH=%USERPROFILE%\anaconda3\Library\mingw-w64\bin"
    set "PATH=%PATH%;%ANACONDA_PATH%"
    goto :found_gcc
)
if exist "C:\ProgramData\anaconda3\Library\mingw-w64\bin\gcc.exe" (
    echo Found MinGW in Anaconda (system install)
    set "PATH=%PATH%;C:\ProgramData\anaconda3\Library\mingw-w64\bin"
    goto :found_gcc
)
REM Try Miniconda
set "MINICONDA_USER=%USERPROFILE%\miniconda3\Library\mingw-w64\bin\gcc.exe"
if exist "%MINICONDA_USER%" (
    echo Found MinGW in Miniconda (user install)
    set "MINICONDA_PATH=%USERPROFILE%\miniconda3\Library\mingw-w64\bin"
    set "PATH=%PATH%;%MINICONDA_PATH%"
    goto :found_gcc
)
if exist "C:\ProgramData\miniconda3\Library\mingw-w64\bin\gcc.exe" (
    echo Found MinGW in Miniconda (system install)
    set "PATH=%PATH%;C:\ProgramData\miniconda3\Library\mingw-w64\bin"
    goto :found_gcc
)
REM Try MSYS2
if exist "C:\msys64\mingw64\bin\gcc.exe" (
    echo Found MinGW in MSYS2
    set "PATH=%PATH%;C:\msys64\mingw64\bin"
    goto :found_gcc
)
REM Try standalone MinGW
if exist "C:\MinGW\bin\gcc.exe" (
    echo Found MinGW standalone
    set "PATH=%PATH%;C:\MinGW\bin"
    goto :found_gcc
)

echo ERROR: GCC not found!
echo Please install MinGW via one of:
echo   - Anaconda: conda install m2w64-toolchain
echo   - MSYS2: https://www.msys2.org/
echo   - Standalone MinGW: https://sourceforge.net/projects/mingw/
echo   - Or add GCC to your PATH
pause
exit /b 1

:found_gcc
REM Verify GCC is now available
where gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: GCC still not found after search
    pause
    exit /b 1
)

echo Using GCC from: 
where gcc

REM Set paths
set SIL_DIR=%~dp0
set DEV_DIR=%SIL_DIR%..
set INC_DIR=%DEV_DIR%\..\inc
set SIL_INC_DIR=%SIL_DIR%inc

REM Output file
set OUTPUT_DLL=%SIL_DIR%libpl_sil.dll

REM Compiler (using gcc/MinGW - adjust if using MSVC)
set CC=gcc

REM Compiler flags (export all symbols for DLL)
set CFLAGS=-shared -O2 -Wall -Wno-unused-variable -static-libgcc -lm -Wl,--export-all-symbols
set INCLUDES=-I"%INC_DIR%" -I"%DEV_DIR%" -I"%SIL_INC_DIR%"

echo.
echo Compiling C files...
echo.

REM Compile all necessary source files
%CC% %CFLAGS% %INCLUDES% ^
    "%DEV_DIR%\powerLimit.c" ^
    "%DEV_DIR%\PID.c" ^
    "%DEV_DIR%\motorController.c" ^
    "%DEV_DIR%\torqueEncoder.c" ^
    "%DEV_DIR%\mathFunctions.c" ^
    "%DEV_DIR%\serial.c" ^
    "%DEV_DIR%\sensors.c" ^
    "%DEV_DIR%\sensorCalculations.c" ^
    "%DEV_DIR%\readyToDriveSound.c" ^
    "%DEV_DIR%\instrumentCluster.c" ^
    "%SIL_DIR%\testHelpers.c" ^
    "%SIL_INC_DIR%\sensors_sil_stubs.c" ^
    "%SIL_INC_DIR%\IO_RTC_sil.c" ^
    "%SIL_INC_DIR%\IO_UART_sil.c" ^
    "%SIL_INC_DIR%\IO_ADC_sil.c" ^
    "%SIL_INC_DIR%\IO_CAN_sil.c" ^
    "%SIL_INC_DIR%\IO_DIO_sil.c" ^
    "%SIL_INC_DIR%\IO_PWM_sil.c" ^
    "%SIL_INC_DIR%\IO_EEPROM_sil.c" ^
    "%SIL_INC_DIR%\IO_POWER_sil.c" ^
    "%SIL_INC_DIR%\IO_PWD_sil.c" ^
    "%SIL_INC_DIR%\IO_WDTimer_sil.c" ^
    "%SIL_INC_DIR%\DIAG_Functions_sil.c" ^
    -o "%OUTPUT_DLL%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build SUCCESS!
    echo Output: %OUTPUT_DLL%
    echo ========================================
    echo.
    echo You can now run: pytest PLTest_Simple.py -v
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
    exit /b 1
)

