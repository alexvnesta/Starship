#!/bin/bash
#
# ci_post_clone.sh
# Xcode Cloud post-clone script
#
# This script runs after Xcode Cloud clones the repository.
# The Xcode project is pre-generated and committed to avoid CI timeouts.
# We fix the hardcoded paths at runtime since CMake embeds absolute paths.
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

    # Fix hardcoded paths from local development machine to CI path
    # CMake embeds absolute paths, so we need to replace them at build time
    echo "Fixing hardcoded paths for CI environment..."

    # Replace the local dev path with the CI repository path
    sed -i '' "s|/Users/alex/development/Starship|${CI_PRIMARY_REPOSITORY_PATH}|g" Starship.xcodeproj/project.pbxproj

    echo "✅ Paths updated for CI"

    # Verify the project is still valid
    if plutil -lint Starship.xcodeproj/project.pbxproj > /dev/null 2>&1; then
        echo "✅ Project file validated"
    else
        echo "⚠️ Warning: Project file may have issues"
    fi
else
    echo "❌ ERROR: Starship.xcodeproj not found!"
    echo "The Xcode project should be pre-generated and committed to the repository."
    echo "Run: cmake -G Xcode -B . -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake -DPLATFORM=OS64"
    exit 1
fi

echo "=== Setup complete ==="
