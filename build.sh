#!/bin/bash
# Build script for openfga_fdw

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_METHOD="${1:-cmake}"

echo "Building openfga_fdw..."
echo "Build method: ${BUILD_METHOD}"
echo "Build type: ${BUILD_TYPE}"

case "${BUILD_METHOD}" in
    cmake)
        echo "Using CMake build system"
        mkdir -p "${SCRIPT_DIR}/build"
        cd "${SCRIPT_DIR}/build"
        cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ..
        cmake --build . -j$(nproc 2>/dev/null || echo 4)
        echo ""
        echo "Build complete. To install:"
        echo "  cd build && sudo cmake --install ."
        ;;

    make|pgxs)
        echo "Using PGXS build system"
        cd "${SCRIPT_DIR}"
        make -j$(nproc 2>/dev/null || echo 4)
        echo ""
        echo "Build complete. To install:"
        echo "  sudo make install"
        ;;

    clean)
        echo "Cleaning build artifacts..."
        rm -rf "${SCRIPT_DIR}/build"
        cd "${SCRIPT_DIR}"
        make clean 2>/dev/null || true
        echo "Clean complete."
        ;;

    *)
        echo "Usage: $0 [cmake|make|clean]"
        echo ""
        echo "Options:"
        echo "  cmake  - Build using CMake (default)"
        echo "  make   - Build using PGXS Makefile"
        echo "  clean  - Clean all build artifacts"
        exit 1
        ;;
esac
