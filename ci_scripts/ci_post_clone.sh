#!/bin/bash
#
# ci_post_clone.sh
# Xcode Cloud post-clone script
#
# This script runs after Xcode Cloud clones the repository.
# It generates the Xcode project using CMake for iOS builds.
#
# Version: 1.0.0 - Initial CI setup test
#

set -e  # Exit on error

echo "=== Starship Xcode Cloud Build Setup ==="
echo "Working directory: $(pwd)"

# Install CMake if not available (Xcode Cloud has Homebrew)
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake..."
    brew install cmake
fi

echo "CMake version: $(cmake --version | head -1)"

# Navigate to repository root
cd "$CI_PRIMARY_REPOSITORY_PATH"
echo "Repository path: $CI_PRIMARY_REPOSITORY_PATH"

# Generate Xcode project for iOS device
echo "=== Generating Xcode project for iOS ==="
cmake -G Xcode \
    -B build-ios-device \
    -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0

echo "=== CMake generation complete ==="
echo "Xcode project created at: build-ios-device/Starship.xcodeproj"

# List the generated files
ls -la build-ios-device/*.xcodeproj || true

echo "=== Setup complete ==="
