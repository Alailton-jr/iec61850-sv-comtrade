@echo off
REM Quick Build and Run script for IEC 61850 SV COMTRADE
REM Builds the project and immediately runs it

echo ================================================================================
echo IEC 61850 SV COMTRADE - Quick Build and Run
echo ================================================================================
echo.

REM Set environment
set PATH=C:\msys64\mingw64\bin;%PATH%
set NPCAP_SDK_DIR=C:\npcap-sdk

REM Build
call build.bat

REM Check if build succeeded
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ================================================================================
echo Running application...
echo ================================================================================
echo.

REM Check if running as Administrator
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Not running as Administrator
    echo Please re-run this script as Administrator for packet operations
    pause
)

cd build

REM Run with any provided arguments, or show help
if "%~1"=="" (
    VirtualTestSet.exe --help
) else (
    VirtualTestSet.exe %*
)

cd ..
pause
