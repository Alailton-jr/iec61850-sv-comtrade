# IEC 61850 SV COMTRADE Converter

Cross-platform tool for converting IEC 61850 Sampled Values to COMTRADE format and replay capabilities.

## 🚀 Platform Support

| Platform | Status | Build Method |
|----------|--------|--------------|
| **Windows** | ✅ Supported | MSYS2/MinGW or Visual Studio with Npcap |
| **Linux** | ✅ Supported | Native GCC/Make with raw sockets |
| **macOS** | ✅ Supported | Native Clang/Make with raw sockets |

## 📋 Quick Start

### Windows (MSYS2 - Recommended)

```bash
# 1. Install MSYS2 from https://www.msys2.org/
# 2. Open MSYS2 MINGW64 terminal
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

# 3. Install Npcap Runtime and SDK from https://npcap.com/#download
# 4. Set environment variable
export NPCAP_SDK_DIR="/c/npcap-sdk"

# 5. Build
./build.sh
```

**See [WINDOWS_SETUP.md](WINDOWS_SETUP.md) for detailed Windows instructions.**

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake

# Build
make

# Run (requires root for raw sockets)
sudo ./build/VirtualTestSet
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Build
make

# Run (requires root for raw sockets)
sudo ./build/VirtualTestSet
```

## 🔧 Build Requirements

### All Platforms
- CMake 3.15+
- C++17 compatible compiler
- Make (or Ninja on Windows)

### Windows-Specific
- **Npcap Runtime** (for packet capture/injection)
- **Npcap SDK** (for compilation)
- **MSYS2** (recommended) or **Visual Studio 2019+**

### Linux-Specific
- GCC 7+ or Clang 5+
- pthread library
- Raw socket support (kernel 2.6+)

### macOS-Specific
- Xcode Command Line Tools
- Raw socket support (macOS 10.12+)

## 📖 Documentation

- **[WINDOWS_SETUP.md](WINDOWS_SETUP.md)** - Complete Windows build guide
- **[WINDOWS_BUILD.md](WINDOWS_BUILD.md)** - Advanced Windows configuration
- **[CROSS_PLATFORM.md](CROSS_PLATFORM.md)** - Cross-platform development notes
- **[INSTALLATION_REQUIREMENTS.txt](INSTALLATION_REQUIREMENTS.txt)** - Dependency details

## 🏗️ Build Options

### Using Make (Linux/macOS)
```bash
make              # Build all targets
make clean        # Clean build artifacts
make rebuild      # Clean and rebuild
```

### Using CMake Directly (All Platforms)
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Windows with Visual Studio
```powershell
# Open Visual Studio Developer Command Prompt
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### Windows with MSYS2/Ninja
```bash
# Open MSYS2 MINGW64 terminal
mkdir build && cd build
cmake -G Ninja ..
ninja
```

## 🔐 Permissions

### Windows
- Run as **Administrator** (required for Npcap packet injection)
- Right-click executable → "Run as administrator"

### Linux/macOS
- Run with **root privileges** (required for raw sockets)
- Use `sudo` or set capabilities:
  ```bash
  sudo setcap cap_net_raw,cap_net_admin=eip ./build/VirtualTestSet
  ```

## 🧪 Running Tests

```bash
# Phasor injection test
./build/phasor_test

# Main application with sample data
./build/VirtualTestSet --input samples/test.cfg
```

## 📂 Project Structure

```
.
├── include/          # Header files
│   ├── raw_socket.h  # Cross-platform socket abstraction
│   ├── timer.h       # Cross-platform timing
│   └── ...
├── src/              # Source files
├── build/            # Build output (generated)
├── CMakeLists.txt    # CMake configuration
├── Makefile          # Unix Makefile wrapper
├── build.sh          # MSYS2 build script
├── build.bat         # Windows batch build script
└── WINDOWS_SETUP.md  # Windows setup guide
```

## 🐛 Troubleshooting

### Windows: "nmake not found" or "CMAKE_CXX_COMPILER not set"
- **Solution:** Use MSYS2 MINGW64 terminal, not regular Command Prompt
- Or use Visual Studio Developer Command Prompt
- See [WINDOWS_SETUP.md](WINDOWS_SETUP.md)

### Windows: "Npcap SDK not found"
- **Solution:** Install Npcap SDK and set `NPCAP_SDK_DIR`:
  ```bash
  export NPCAP_SDK_DIR="/c/npcap-sdk"  # MSYS2
  ```
  ```powershell
  $env:NPCAP_SDK_DIR="C:\npcap-sdk"    # PowerShell
  ```

### Linux/macOS: "Permission denied" when running
- **Solution:** Run with sudo or set capabilities (Linux only):
  ```bash
  sudo setcap cap_net_raw,cap_net_admin=eip ./build/VirtualTestSet
  ```

### Any Platform: Build fails with dependency errors
- **Solution:** Check [INSTALLATION_REQUIREMENTS.txt](INSTALLATION_REQUIREMENTS.txt)
- Ensure all required libraries are installed

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Test on your target platform(s)
4. Submit a pull request

## 📄 License

[Your License Here]

## 🔗 Links

- **Npcap:** https://npcap.com/
- **MSYS2:** https://www.msys2.org/
- **CMake:** https://cmake.org/
- **IEC 61850 Standard:** https://en.wikipedia.org/wiki/IEC_61850

---

**Note:** For production use, ensure you have proper permissions to capture network traffic and comply with local network security policies.
