#!/bin/bash
# MSYS2 Build Script for IEC 61850 SV COMTRADE

set -e  # Exit on error

echo "================================================================================"
echo "IEC 61850 SV COMTRADE - MSYS2 Build Script"
echo "================================================================================"
echo

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "[ERROR] $1 not found!"
        echo "Please install with: pacman -S $2"
        exit 1
    fi
    echo "[OK] $1 found"
}

check_tool "g++" "mingw-w64-x86_64-toolchain"
check_tool "cmake" "mingw-w64-x86_64-cmake"
check_tool "ninja" "mingw-w64-x86_64-ninja"

# Check for Npcap SDK
if [ -z "$NPCAP_SDK_DIR" ]; then
    if [ -d "/c/npcap-sdk" ]; then
        export NPCAP_SDK_DIR="/c/npcap-sdk"
        echo "[INFO] Using default Npcap SDK path: /c/npcap-sdk"
    else
        echo "[WARNING] NPCAP_SDK_DIR not set and /c/npcap-sdk not found"
        echo "Please install Npcap SDK from: https://npcap.com/#download"
        echo "Extract to C:\\npcap-sdk and run:"
        echo "  export NPCAP_SDK_DIR=\"/c/npcap-sdk\""
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
else
    echo "[OK] Npcap SDK path: $NPCAP_SDK_DIR"
fi

echo
echo "================================================================================"
echo "Configuring with CMake..."
echo "================================================================================"
echo

# Create build directory
mkdir -p build
cd build

# Configure
cmake -G Ninja ..

echo
echo "================================================================================"
echo "Building with Ninja..."
echo "================================================================================"
echo

# Build
ninja -v

echo
echo "================================================================================"
echo "Build complete!"
echo "================================================================================"
echo
echo "Executable: build/VirtualTestSet.exe"
echo

# Ask if user wants to run
read -p "Do you want to run the application? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo
    echo "Running VirtualTestSet.exe..."
    echo "NOTE: Administrator privileges may be required for packet capture"
    echo
    ./VirtualTestSet.exe --help
    echo
    echo "To run with options, use:"
    echo "  ./VirtualTestSet.exe [options]"
fi
echo
