#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! command -v node &> /dev/null; then
    echo "Error: Node.js not found"
    exit 1
fi

if [ ! -d "node_modules" ]; then
    echo "Installing npm dependencies..."
    npm install
fi

echo "Building native module with node-gyp..."
npm run build 2>&1

echo ""
echo "Running JavaScript exception tests..."
node test_exceptions.js
