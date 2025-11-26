# Windows Setup Guide (MSYS2 Method - Recommended)

This is the **easiest and fastest** way to build on Windows.

## Step 1: Install MSYS2

1. Download MSYS2 from: **https://www.msys2.org/**
2. Run the installer (e.g., `msys2-x86_64-20231026.exe`)
3. Install to default location: `C:\msys64`
4. After installation completes, open **"MSYS2 MINGW64"** from the Start menu

## Step 2: Install Build Tools

In the MSYS2 MINGW64 terminal, run these commands:

```bash
# Update package database
pacman -Syu
```

**Important:** Close and reopen the MSYS2 MINGW64 terminal after the first update, then continue:

```bash
# Complete the update
pacman -Su

# Install all required build tools
pacman -S --needed base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja git
```

This installs:
- GCC/G++ compiler (MinGW-w64)
- CMake build system
- Ninja build tool
- Git version control
- Make and other build utilities

## Step 3: Install Npcap

### 3a. Install Npcap Runtime
1. Download Npcap from: **https://npcap.com/#download**
2. Run installer as Administrator
3. **Important:** Check **"Install Npcap in WinPcap API-compatible Mode"**
4. Complete installation

### 3b. Install Npcap SDK
1. Download Npcap SDK from the same page: **https://npcap.com/#download**
2. Extract the ZIP file (e.g., `npcap-sdk-1.13.zip`)
3. Extract to: `C:\npcap-sdk`

   Your directory structure should look like:
   ```
   C:\npcap-sdk\
   ├── Include\
   │   └── pcap\
   │       └── pcap.h
   └── Lib\
       └── x64\
           └── wpcap.lib
   ```

## Step 4: Set Environment Variable

In the MSYS2 MINGW64 terminal:

```bash
# Set Npcap SDK path
export NPCAP_SDK_DIR="/c/npcap-sdk"

# Add to your ~/.bashrc to make it permanent (optional)
echo 'export NPCAP_SDK_DIR="/c/npcap-sdk"' >> ~/.bashrc
```

## Step 5: Build the Project

```bash
# Navigate to your project (adjust path as needed)
cd /d/Github/iec61850-sv-comtrade

# Create build directory
mkdir -p build
cd build

# Configure with CMake using Ninja generator
cmake -G Ninja ..

# Build the project
ninja

# Or build with verbose output to see what's happening
ninja -v
```

## Step 6: Run the Application

```bash
# Still in the build directory
./VirtualTestSet.exe --help
```

**Note:** You must run the application as Administrator to capture/inject packets on Windows.

---

## Alternative Method: Visual Studio

If you prefer Visual Studio:

1. Install **Visual Studio 2022 Community** (free)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - During installation, select **"Desktop development with C++"**

2. Install **CMake** (if not included)
   - Download from: https://cmake.org/download/
   - Add to system PATH during installation

3. Install Npcap and Npcap SDK (Steps 3a and 3b above)

4. Set Environment Variable in Windows:
   - Open System Properties → Advanced → Environment Variables
   - Add new System Variable:
     - Name: `NPCAP_SDK_DIR`
     - Value: `C:\npcap-sdk`

5. Open **Visual Studio Command Prompt** (or PowerShell with VS environment):

   ```powershell
   # Navigate to project
   cd D:\Github\iec61850-sv-comtrade
   
   # Create build directory
   mkdir build
   cd build
   
   # Configure
   cmake -G "Visual Studio 17 2022" -A x64 ..
   
   # Build
   cmake --build . --config Release
   ```

---

## Troubleshooting

### Error: "nmake not found"
- You're trying to use the wrong generator. Use `-G Ninja` or `-G "Visual Studio 17 2022"`

### Error: "CMAKE_CXX_COMPILER not set"
- Make sure you're using MSYS2 MINGW64 terminal (not MSYS2 MSYS terminal)
- Or use Visual Studio Command Prompt if using Visual Studio

### Error: "Npcap SDK not found"
- Check that `NPCAP_SDK_DIR` is set correctly
- In MSYS2: `echo $NPCAP_SDK_DIR` should show `/c/npcap-sdk`
- Verify the directory structure matches Step 3b above

### Error: "Cannot open include file: 'pcap/pcap.h'"
- Npcap SDK not installed correctly
- Reinstall Npcap SDK to `C:\npcap-sdk`

### Error: "wpcap.lib not found"
- Make sure you downloaded the **SDK**, not just the runtime installer
- Check that `C:\npcap-sdk\Lib\x64\wpcap.lib` exists

### Application runs but cannot capture packets
- Run as Administrator (required for raw socket access)
- Check that Npcap service is running: `sc query npcap`
- Verify Npcap is installed in WinPcap-compatible mode

---

## Quick Reference

### MSYS2 MINGW64 Build Commands
```bash
cd /d/Github/iec61850-sv-comtrade
mkdir -p build && cd build
export NPCAP_SDK_DIR="/c/npcap-sdk"
cmake -G Ninja ..
ninja
```

### Visual Studio Build Commands
```powershell
cd D:\Github\iec61850-sv-comtrade
mkdir build; cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

---

## Why MSYS2 is Recommended

- ✅ Easier setup (all tools in one package manager)
- ✅ Familiar Unix-like environment
- ✅ Better CMake integration
- ✅ Faster compilation with Ninja
- ✅ No need for large Visual Studio installation
- ✅ Package manager (pacman) for easy updates
