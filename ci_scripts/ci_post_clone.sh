#!/bin/bash
#
# ci_post_clone.sh
# Xcode Cloud post-clone script
#
# This script runs after Xcode Cloud clones the repository.
# It runs CMake to download dependencies and generate the Xcode project.
# The post_clone phase has a 25-minute timeout which should be sufficient.
#

set -e  # Exit on error

echo "=== Starship Xcode Cloud Build Setup ==="
echo "Working directory: $(pwd)"
echo "Start time: $(date)"

# Navigate to repository root
cd "$CI_PRIMARY_REPOSITORY_PATH"
echo "Repository path: $CI_PRIMARY_REPOSITORY_PATH"

# Install CMake if not available
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake via Homebrew..."
    brew install cmake
fi

echo "CMake version: $(cmake --version | head -1)"

# Generate Xcode project with CMake
# This downloads all dependencies and creates the project
echo "=== Running CMake to generate Xcode project ==="
echo "This may take several minutes to download dependencies..."

cmake -G Xcode \
    -B . \
    -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON

echo "✅ CMake configuration complete"
echo "End time: $(date)"

# Verify the project was generated
if [ -f "Starship.xcodeproj/project.pbxproj" ]; then
    echo "✅ Xcode project generated successfully"
    ls -la Starship.xcodeproj/
else
    echo "❌ ERROR: Xcode project was not generated!"
    exit 1
fi

echo "=== Setup complete ==="
