#!/bin/bash
# Quick Build and Run script for IEC 61850 SV COMTRADE

echo "================================================================================"
echo "IEC 61850 SV COMTRADE - Quick Build and Run"
echo "================================================================================"
echo

# Set environment
export NPCAP_SDK_DIR="/c/npcap-sdk"

# Build
./build.sh

# Check if build succeeded
if [ $? -ne 0 ]; then
    echo
    echo "[ERROR] Build failed!"
    exit 1
fi

echo
echo "================================================================================"
echo "Running application..."
echo "================================================================================"
echo

cd build

# Run with any provided arguments, or show help
if [ $# -eq 0 ]; then
    ./VirtualTestSet.exe --help
else
    ./VirtualTestSet.exe "$@"
fi

cd ..
