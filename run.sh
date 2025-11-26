#!/bin/bash
# Run script for IEC 61850 SV COMTRADE Application

echo "================================================================================"
echo "IEC 61850 SV COMTRADE - Run Script"
echo "================================================================================"
echo

# Check if executable exists
if [ ! -f "build/VirtualTestSet.exe" ]; then
    echo "[ERROR] VirtualTestSet.exe not found!"
    echo "Please build the project first using ./build.sh"
    exit 1
fi

echo "[OK] Executable found"
echo

# Check if running with sufficient privileges (Windows doesn't need sudo check)
echo "[INFO] On Windows, make sure to run MSYS2 terminal as Administrator"
echo "      for packet capture/injection operations"
echo

cd build

# If no arguments provided, show help
if [ $# -eq 0 ]; then
    echo "No arguments provided. Showing help:"
    echo
    ./VirtualTestSet.exe --help
    echo
    echo
    echo "Example usage:"
    echo "  ./run.sh --interface eth0"
    echo "  ./run.sh --help"
else
    echo "Running: VirtualTestSet.exe $@"
    echo
    ./VirtualTestSet.exe "$@"
fi

cd ..
