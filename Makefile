# ============================================================================
# IEC 61850 SV COMTRADE - Makefile
# ============================================================================
# This Makefile provides convenient targets for:
# - Installing system dependencies
# - Building the project
# - Running the executables
# - Cleaning build artifacts
# ============================================================================

.PHONY: help install-deps install-deps-ubuntu install-deps-fedora install-deps-arch
.PHONY: build rebuild clean distclean configure debug release
.PHONY: run run-phasor permissions install uninstall test

# ============================================================================
# Configuration
# ============================================================================

PROJECT_NAME := VirtualTestSet
BUILD_DIR := build
BUILD_TYPE ?= Release
CMAKE_FLAGS ?=
NPROC := $(shell nproc 2>/dev/null || echo 4)

# Detect Linux distribution
DISTRO := $(shell if [ -f /etc/os-release ]; then . /etc/os-release && echo $$ID; else echo unknown; fi)

# ============================================================================
# Help Target
# ============================================================================

help:
	@echo "============================================================================"
	@echo "IEC 61850 SV COMTRADE - Makefile Targets"
	@echo "============================================================================"
	@echo ""
	@echo "Installation:"
	@echo "  make install-deps        - Auto-detect and install system dependencies"
	@echo "  make install-deps-ubuntu - Install dependencies on Ubuntu/Debian"
	@echo "  make install-deps-fedora - Install dependencies on Fedora/RHEL/CentOS"
	@echo "  make install-deps-arch   - Install dependencies on Arch Linux"
	@echo ""
	@echo "Building:"
	@echo "  make build              - Build the project (default: Release)"
	@echo "  make debug              - Build with debug symbols"
	@echo "  make release            - Build optimized release version"
	@echo "  make rebuild            - Clean and rebuild"
	@echo "  make configure          - Run CMake configuration only"
	@echo ""
	@echo "Running:"
	@echo "  make run                - Run VirtualTestSet (main application)"
	@echo "  make run-phasor         - Run phasor_test executable"
	@echo "  make permissions        - Grant CAP_NET_RAW capability to executables"
	@echo ""
	@echo "Maintenance:"
	@echo "  make clean              - Remove build artifacts"
	@echo "  make distclean          - Remove build directory completely"
	@echo "  make install            - Install executables system-wide"
	@echo "  make uninstall          - Remove installed executables"
	@echo ""
	@echo "Examples:"
	@echo "  make install-deps build permissions  - Full setup"
	@echo "  make BUILD_TYPE=Debug build          - Build debug version"
	@echo "  make rebuild run                     - Rebuild and run"
	@echo ""
	@echo "============================================================================"

# ============================================================================
# Dependency Installation
# ============================================================================

install-deps:
	@echo "Detected distribution: $(DISTRO)"
	@if [ "$(DISTRO)" = "ubuntu" ] || [ "$(DISTRO)" = "debian" ] || [ "$(DISTRO)" = "linuxmint" ]; then \
		$(MAKE) install-deps-ubuntu; \
	elif [ "$(DISTRO)" = "fedora" ] || [ "$(DISTRO)" = "rhel" ] || [ "$(DISTRO)" = "centos" ] || [ "$(DISTRO)" = "rocky" ]; then \
		$(MAKE) install-deps-fedora; \
	elif [ "$(DISTRO)" = "arch" ] || [ "$(DISTRO)" = "manjaro" ]; then \
		$(MAKE) install-deps-arch; \
	else \
		echo "Unknown distribution. Please run one of:"; \
		echo "  make install-deps-ubuntu"; \
		echo "  make install-deps-fedora"; \
		echo "  make install-deps-arch"; \
		exit 1; \
	fi

install-deps-ubuntu:
	@echo "============================================================================"
	@echo "Installing dependencies for Ubuntu/Debian..."
	@echo "============================================================================"
	sudo apt-get update
	sudo apt-get install -y build-essential cmake git
	sudo apt-get install -y linux-headers-$$(uname -r)
	@echo ""
	@echo "Dependencies installed successfully!"
	@echo ""

install-deps-fedora:
	@echo "============================================================================"
	@echo "Installing dependencies for Fedora/RHEL/CentOS..."
	@echo "============================================================================"
	sudo dnf groupinstall -y "Development Tools"
	sudo dnf install -y cmake git
	sudo dnf install -y kernel-headers kernel-devel
	@echo ""
	@echo "Dependencies installed successfully!"
	@echo ""

install-deps-arch:
	@echo "============================================================================"
	@echo "Installing dependencies for Arch Linux..."
	@echo "============================================================================"
	sudo pacman -Syu --noconfirm
	sudo pacman -S --needed --noconfirm base-devel cmake git
	sudo pacman -S --needed --noconfirm linux-headers
	@echo ""
	@echo "Dependencies installed successfully!"
	@echo ""

# ============================================================================
# Build Targets
# ============================================================================

configure:
	@echo "============================================================================"
	@echo "Configuring project (Build Type: $(BUILD_TYPE))..."
	@echo "============================================================================"
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS) ..
	@echo ""
	@echo "Configuration complete!"
	@echo ""

build: configure
	@echo "============================================================================"
	@echo "Building project..."
	@echo "============================================================================"
	cd $(BUILD_DIR) && $(MAKE) -j$(NPROC)
	@echo ""
	@echo "Build complete!"
	@echo "Executables:"
	@echo "  - $(BUILD_DIR)/$(PROJECT_NAME)"
	@echo "  - $(BUILD_DIR)/phasor_test"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Grant permissions: make permissions"
	@echo "  2. Run application:   make run"
	@echo ""

debug:
	@$(MAKE) BUILD_TYPE=Debug build

release:
	@$(MAKE) BUILD_TYPE=Release build

rebuild: clean build

# ============================================================================
# Runtime Targets
# ============================================================================

permissions:
	@echo "============================================================================"
	@echo "Granting CAP_NET_RAW capability to executables..."
	@echo "============================================================================"
	@if [ ! -f $(BUILD_DIR)/$(PROJECT_NAME) ]; then \
		echo "Error: $(PROJECT_NAME) not found. Run 'make build' first."; \
		exit 1; \
	fi
	sudo setcap cap_net_raw+ep $(BUILD_DIR)/$(PROJECT_NAME)
	@if [ -f $(BUILD_DIR)/phasor_test ]; then \
		sudo setcap cap_net_raw+ep $(BUILD_DIR)/phasor_test; \
	fi
	@echo ""
	@echo "Permissions granted! You can now run without sudo."
	@echo ""

run:
	@echo "============================================================================"
	@echo "Running $(PROJECT_NAME)..."
	@echo "============================================================================"
	@if [ ! -f $(BUILD_DIR)/$(PROJECT_NAME) ]; then \
		echo "Error: $(PROJECT_NAME) not found. Run 'make build' first."; \
		exit 1; \
	fi
	@echo ""
	cd $(BUILD_DIR) && ./$(PROJECT_NAME)

run-phasor:
	@echo "============================================================================"
	@echo "Running phasor_test..."
	@echo "============================================================================"
	@if [ ! -f $(BUILD_DIR)/phasor_test ]; then \
		echo "Error: phasor_test not found. Run 'make build' first."; \
		exit 1; \
	fi
	@echo ""
	cd $(BUILD_DIR) && ./phasor_test

# ============================================================================
# Installation Targets
# ============================================================================

install:
	@echo "============================================================================"
	@echo "Installing executables system-wide..."
	@echo "============================================================================"
	@if [ ! -d $(BUILD_DIR) ]; then \
		echo "Error: Build directory not found. Run 'make build' first."; \
		exit 1; \
	fi
	cd $(BUILD_DIR) && sudo $(MAKE) install
	@echo ""
	@echo "Executables installed to /usr/local/bin (or system default)"
	@echo ""

uninstall:
	@echo "============================================================================"
	@echo "Uninstalling executables..."
	@echo "============================================================================"
	@if [ ! -d $(BUILD_DIR) ]; then \
		echo "Error: Build directory not found."; \
		exit 1; \
	fi
	cd $(BUILD_DIR) && sudo xargs rm -f < install_manifest.txt 2>/dev/null || \
		echo "No install manifest found or already uninstalled."
	@echo ""
	@echo "Uninstall complete!"
	@echo ""

build-run:
	@$(MAKE) build run
# ============================================================================
# Cleaning Targets
# ============================================================================

clean:
	@echo "============================================================================"
	@echo "Cleaning build artifacts..."
	@echo "============================================================================"
	@if [ -d $(BUILD_DIR) ]; then \
		cd $(BUILD_DIR) && $(MAKE) clean 2>/dev/null || rm -f *.o $(PROJECT_NAME) phasor_test; \
		echo "Build artifacts cleaned."; \
	else \
		echo "Build directory does not exist. Nothing to clean."; \
	fi
	@echo ""

distclean:
	@echo "============================================================================"
	@echo "Removing build directory..."
	@echo "============================================================================"
	rm -rf $(BUILD_DIR)
	rm -f compile_commands.json
	@echo ""
	@echo "Build directory removed!"
	@echo ""

# ============================================================================
# Quick Setup Target
# ============================================================================

setup: install-deps build permissions
	@echo "============================================================================"
	@echo "Setup complete! Your project is ready to run."
	@echo "============================================================================"
	@echo ""
	@echo "Try running:"
	@echo "  make run         - Run the main application"
	@echo "  make run-phasor  - Run the phasor test"
	@echo ""

# ============================================================================
# Info Target
# ============================================================================

info:
	@echo "============================================================================"
	@echo "Project Information"
	@echo "============================================================================"
	@echo "Project Name:    $(PROJECT_NAME)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Build Type:      $(BUILD_TYPE)"
	@echo "Parallel Jobs:   $(NPROC)"
	@echo "Distribution:    $(DISTRO)"
	@echo ""
	@echo "CMake version:"
	@cmake --version 2>/dev/null || echo "  CMake not installed"
	@echo ""
	@echo "GCC version:"
	@gcc --version 2>/dev/null | head -n1 || echo "  GCC not installed"
	@echo ""
	@echo "Build status:"
	@if [ -f $(BUILD_DIR)/$(PROJECT_NAME) ]; then \
		echo "  ✓ $(PROJECT_NAME) built"; \
	else \
		echo "  ✗ $(PROJECT_NAME) not built"; \
	fi
	@if [ -f $(BUILD_DIR)/phasor_test ]; then \
		echo "  ✓ phasor_test built"; \
	else \
		echo "  ✗ phasor_test not built"; \
	fi
	@echo ""
	@echo "Network interfaces:"
	@ip link show 2>/dev/null | grep -E "^[0-9]+:" | awk '{print "  " $$2}' | sed 's/:$$//' || echo "  Unable to list interfaces"
	@echo ""
	@echo "============================================================================"
