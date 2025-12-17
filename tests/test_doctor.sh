#!/bin/bash
# Test script for mirror_bridge_doctor
# Verifies that the doctor tool runs and produces expected output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCTOR="$SCRIPT_DIR/mirror_bridge_doctor"

echo "Testing mirror_bridge_doctor..."
echo ""

# Test 1: Help message
echo "Test 1: Help message..."
if $DOCTOR --help | grep -q "Mirror Bridge Doctor"; then
    echo "✓ Help message works"
else
    echo "✗ Help message failed"
    exit 1
fi

# Test 2: Basic execution
echo "Test 2: Basic execution..."
OUTPUT=$($DOCTOR 2>&1) || true  # Don't fail on non-zero exit

if echo "$OUTPUT" | grep -q "Mirror Bridge Doctor"; then
    echo "✓ Doctor runs and shows title"
else
    echo "✗ Doctor failed to show title"
    exit 1
fi

# Test 3: Check sections exist
echo "Test 3: Checking output sections..."
SECTIONS=("C++ Compiler" "Reflection Support" "Python Environment" "Summary")
for section in "${SECTIONS[@]}"; do
    if echo "$OUTPUT" | grep -q "$section"; then
        echo "✓ Section found: $section"
    else
        echo "✗ Section missing: $section"
        exit 1
    fi
done

# Test 4: Verbose mode
echo "Test 4: Verbose mode..."
VERBOSE_OUTPUT=$($DOCTOR --verbose 2>&1) || true
if [ ${#VERBOSE_OUTPUT} -gt ${#OUTPUT} ]; then
    echo "✓ Verbose mode produces more output"
else
    echo "⚠ Verbose mode may not be adding extra output"
fi

# Test 5: Summary counts
echo "Test 5: Summary contains counts..."
if echo "$OUTPUT" | grep -qE "Passed:.*[0-9]+"; then
    echo "✓ Summary shows pass count"
else
    echo "✗ Summary missing pass count"
    exit 1
fi

echo ""
echo "All doctor tests passed!"
