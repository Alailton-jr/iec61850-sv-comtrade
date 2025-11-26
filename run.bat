@echo off
REM Run script for IEC 61850 SV COMTRADE Application
REM This script runs the built application with proper environment setup

echo ================================================================================
echo IEC 61850 SV COMTRADE - Run Script
echo ================================================================================
echo.

REM Check if executable exists
if not exist "build\VirtualTestSet.exe" (
    echo [ERROR] VirtualTestSet.exe not found!
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

REM Check if running as Administrator
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Not running as Administrator
    echo Packet capture/injection requires Administrator privileges
    echo.
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo [OK] Running as Administrator
echo.

REM Set PATH for MinGW DLLs if needed
set PATH=C:\msys64\mingw64\bin;%PATH%

cd build

REM If no arguments provided, show help
if "%~1"=="" (
    echo No arguments provided. Showing help:
    echo.
    VirtualTestSet.exe --help
    echo.
    echo.
    echo Example usage:
    echo   run.bat --interface eth0
    echo   run.bat --help
    pause
) else (
    echo Running: VirtualTestSet.exe %*
    echo.
    VirtualTestSet.exe %*
    pause
)

cd ..
