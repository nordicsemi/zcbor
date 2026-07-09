#!/bin/bash

set -e

echo "zcbor installed to $INSTALL_DIR"

# Build and install zcbor to a custom directory (default: system paths)
if [ -n "$1" ]; then
	INSTALL_DIR="$1"
	INSTALL_ARG="-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"
else
	INSTALL_ARG=""
fi

BUILD_DIR=$(pwd)/_build

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
cmake $INSTALL_ARG ..

# Build and install
cmake --build . --target install

if [ -n "$INSTALL_DIR" ]; then
	echo "zcbor installed to $INSTALL_DIR"
else
	echo "zcbor installed to system default paths (e.g. /usr/local)"
fi
