#!/bin/bash
# Build and test the JavaScript Vec3 module using node-gyp
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check for node and npm
if ! command -v node &> /dev/null; then
    echo "Error: Node.js not found"
    exit 1
fi

if ! command -v npm &> /dev/null; then
    echo "Error: npm not found"
    exit 1
fi

echo "Node.js version: $(node --version)"
echo "npm version: $(npm --version)"

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
    echo "Installing npm dependencies..."
    npm install
fi

# Build the native module
echo ""
echo "Building native module with node-gyp..."
npm run build 2>&1 || {
    echo "Note: node-gyp build may fail in CI without full build toolchain"
    echo "Checking for pre-built module..."
    if [ -f "build/Release/vec3.node" ]; then
        echo "Found pre-built module"
    else
        echo "No pre-built module found, skipping test"
        exit 0
    fi
}

# Run the test
echo ""
echo "Running JavaScript tests..."
node test_vec3.js

echo ""
echo "✓ All JavaScript tests passed"
