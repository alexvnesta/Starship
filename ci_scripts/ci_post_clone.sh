#!/bin/bash
#
# ci_post_clone.sh
# Xcode Cloud post-clone script
#
# This script runs after Xcode Cloud clones the repository.
# The Xcode project is pre-generated and committed to avoid CI timeouts.
#

set -e  # Exit on error

echo "=== Starship Xcode Cloud Build Setup ==="
echo "Working directory: $(pwd)"

# Navigate to repository root
cd "$CI_PRIMARY_REPOSITORY_PATH"
echo "Repository path: $CI_PRIMARY_REPOSITORY_PATH"

# Verify the pre-generated Xcode project exists
if [ -f "Starship.xcodeproj/project.pbxproj" ]; then
    echo "✅ Pre-generated Xcode project found"
    ls -la Starship.xcodeproj/
else
    echo "❌ ERROR: Starship.xcodeproj not found!"
    echo "The Xcode project should be pre-generated and committed to the repository."
    echo "Run: cmake -G Xcode -B . -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=OS64"
    exit 1
fi

echo "=== Setup complete ==="
