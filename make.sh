#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring miqubg ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" "$@"

echo "==> Building miqubg..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binary is at $BUILD_DIR/miqubg"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing miqubg to system (/usr/bin)..."
    cmake --install "$BUILD_DIR"
    echo "==> miqubg successfully installed to /usr/bin!"
else
    echo "==> Built locally in $BUILD_DIR. To install system-wide, run: sudo ./make.sh"
fi
