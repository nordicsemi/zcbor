#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: build_zcbor.sh [INSTALL_DIR]

Build and install zcbor to a custom directory (default: ./install)

Positional arguments:
  INSTALL_DIR        Installation prefix directory (default: ./install)

Options:
  -h, --help         Show this help message and exit

Examples:
  build_zcbor.sh
  build_zcbor.sh /home/user/.local
EOF
}

# Show help and exit
if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
	usage
	exit 0
fi

# Build and install zcbor to a custom directory (default: ./install)
if [ -n "${1:-}" ]; then
	INSTALL_DIR="$1"
else
	INSTALL_DIR="$(pwd)/install"
fi

# Build directory (match README examples)
BUILD_DIR="$(pwd)/build"

echo "Configuring zcbor in: $BUILD_DIR"
echo "Install prefix: $INSTALL_DIR"

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
 
# Configure (out-of-source build per README)
cmake -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" .

# Build
cmake --build "$BUILD_DIR"

# Install
cmake --install "$BUILD_DIR"

echo "zcbor installed to $INSTALL_DIR"
