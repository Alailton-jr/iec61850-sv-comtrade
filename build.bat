@echo off
REM Windows Build Script for IEC 61850 SV COMTRADE
REM This script helps configure and build the project on Windows

echo ================================================================================
echo IEC 61850 SV COMTRADE - Windows Build Script
echo ================================================================================
echo.

REM Check if running in Visual Studio Developer Command Prompt
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] Visual Studio compiler detected
    set USE_VS=1
    goto :check_cmake
)

REM Check if running in MSYS2 MINGW64 environment
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] MinGW-w64 compiler detected
    set USE_MINGW=1
    goto :check_cmake
)

echo [ERROR] No C++ compiler found!
echo.
echo Please use one of these methods:
echo   1. Open "MSYS2 MINGW64" terminal and run: ./build.sh
echo   2. Open "Developer Command Prompt for VS" and run: build.bat
echo.
echo See WINDOWS_SETUP.md for installation instructions.
pause
exit /b 1

:check_cmake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found!
    echo Please install CMake from: https://cmake.org/download/
    pause
    exit /b 1
)
echo [OK] CMake found

REM Check for Npcap SDK
if not defined NPCAP_SDK_DIR (
    if exist "C:\npcap-sdk" (
        set NPCAP_SDK_DIR=C:\npcap-sdk
        echo [INFO] Using default Npcap SDK path: C:\npcap-sdk
    ) else (
        echo [WARNING] NPCAP_SDK_DIR not set and C:\npcap-sdk not found
        echo Please install Npcap SDK from: https://npcap.com/#download
        echo and set NPCAP_SDK_DIR environment variable
        pause
    )
) else (
    echo [OK] Npcap SDK path: %NPCAP_SDK_DIR%
)

echo.
echo ================================================================================
echo Starting build...
echo ================================================================================
echo.

REM Create and enter build directory
if not exist build mkdir build
cd build

REM Configure based on detected compiler
if defined USE_VS (
    echo Configuring with Visual Studio...
    cmake -G "Visual Studio 17 2022" -A x64 ..
    if %ERRORLEVEL% NEQ 0 goto :build_error
    
    echo.
    echo Building with Visual Studio...
    cmake --build . --config Release
    if %ERRORLEVEL% NEQ 0 goto :build_error
    
    echo.
    echo ================================================================================
    echo Build complete!
    echo Executable: build\Release\VirtualTestSet.exe
    echo ================================================================================
    
    REM Ask if user wants to run
    set /p RUN_APP="Do you want to run the application? (y/N): "
    if /i "%RUN_APP%"=="y" (
        echo.
        echo Running VirtualTestSet.exe...
        echo NOTE: Administrator privileges may be required for packet capture
        echo.
        cd Release
        VirtualTestSet.exe
        cd ..
    )
) else if defined USE_MINGW (
    echo Configuring with Ninja...
    cmake -G Ninja -DPCAP_LIBRARY="C:/npcap-sdk/Lib/x64/libwpcap.a" ..
    if %ERRORLEVEL% NEQ 0 goto :build_error
    
    echo.
    echo Building with Ninja...
    ninja
    if %ERRORLEVEL% NEQ 0 goto :build_error
    
    echo.
    echo ================================================================================
    echo Build complete!
    echo Executable: build\VirtualTestSet.exe
    echo ================================================================================
    
    REM Ask if user wants to run
    set /p RUN_APP="Do you want to run the application? (y/N): "
    if /i "%RUN_APP%"=="y" (
        echo.
        echo Running VirtualTestSet.exe...
        echo NOTE: Administrator privileges may be required for packet capture
        echo.
        VirtualTestSet.exe
    )
)

cd ..
pause
exit /b 0

:build_error
echo.
echo ================================================================================
echo [ERROR] Build failed!
echo ================================================================================
echo.
echo Check the error messages above for details.
echo See WINDOWS_SETUP.md for troubleshooting help.
echo.
cd ..
pause
exit /b 1
