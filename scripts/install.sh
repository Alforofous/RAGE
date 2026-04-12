#!/bin/bash
set -e

# RAGE dependency installer (Windows)
# Run once to set up the development environment.

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[OK]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!!]${NC} $1"; }
fail()  { echo -e "${RED}[FAIL]${NC} $1"; exit 1; }
ask()   { echo -en "${YELLOW}[??]${NC} $1 (y/n) "; read -r ans; [[ "$ans" == "y" || "$ans" == "Y" ]]; }

echo "=== RAGE Dependency Installer ==="
echo ""

# --- Clang ---
echo "Checking C++ compiler..."
if command -v clang++ &>/dev/null; then
    info "clang++ found: $(clang++ --version | head -1)"
elif command -v g++ &>/dev/null; then
    info "g++ found: $(g++ --version | head -1)"
else
    warn "No C++ compiler found."
    if ask "Install LLVM/Clang?"; then
        CLANG_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/LLVM-17.0.6-win64.exe"
        INSTALLER="$TEMP/clang-installer.exe"
        echo "Downloading Clang..."
        curl -L -o "$INSTALLER" "$CLANG_URL"
        echo "Installing Clang (silent)..."
        "$INSTALLER" /S
        rm -f "$INSTALLER"
        info "Clang installed. Restart your terminal to update PATH."
    else
        fail "C++ compiler is required."
    fi
fi

echo ""

# --- CMake ---
echo "Checking CMake..."
if command -v cmake &>/dev/null; then
    info "cmake found: $(cmake --version | head -1)"
else
    warn "CMake not found."
    if ask "Install CMake?"; then
        CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-windows-x86_64.msi"
        INSTALLER="$TEMP/cmake-installer.msi"
        echo "Downloading CMake..."
        curl -L -o "$INSTALLER" "$CMAKE_URL"
        echo "Installing CMake (silent)..."
        msiexec.exe /i "$(cygpath -w "$INSTALLER")" /quiet /norestart
        rm -f "$INSTALLER"
        info "CMake installed. Restart your terminal to update PATH."
    else
        fail "CMake is required."
    fi
fi

echo ""

# --- Vulkan SDK ---
echo "Checking Vulkan SDK..."
if [[ -n "$VULKAN_SDK" && -f "$VULKAN_SDK/Include/vulkan/vulkan.h" ]]; then
    info "Vulkan SDK found: $VULKAN_SDK"
else
    # Check common paths
    FOUND=""
    for BASE in "/c/VulkanSDK" "/c/Program Files/VulkanSDK"; do
        if [[ -d "$BASE" ]]; then
            LATEST=$(ls -d "$BASE"/1.* 2>/dev/null | sort -V | tail -1)
            if [[ -n "$LATEST" && -f "$LATEST/Include/vulkan/vulkan.h" ]]; then
                FOUND="$LATEST"
                break
            fi
        fi
    done

    if [[ -n "$FOUND" ]]; then
        info "Vulkan SDK found: $FOUND"
        warn "Set VULKAN_SDK=$FOUND in your environment for CMake."
    else
        warn "Vulkan SDK not found."
        if ask "Install Vulkan SDK?"; then
            VULKAN_URL="https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe"
            INSTALLER="$TEMP/vulkan-sdk.exe"
            echo "Downloading Vulkan SDK..."
            curl -L -o "$INSTALLER" "$VULKAN_URL"
            echo "Installing Vulkan SDK (silent)..."
            "$INSTALLER" --accept-licenses --default-answer --quiet
            rm -f "$INSTALLER"
            info "Vulkan SDK installed. Restart your terminal to update environment."
        else
            fail "Vulkan SDK is required."
        fi
    fi
fi

echo ""

# --- Ninja (build system) ---
echo "Checking Ninja..."
if command -v ninja &>/dev/null; then
    info "ninja found: $(ninja --version)"
else
    warn "Ninja not found. CMake will fall back to other generators."
fi

echo ""
info "All dependencies checked. Run scripts/build.sh to build."
