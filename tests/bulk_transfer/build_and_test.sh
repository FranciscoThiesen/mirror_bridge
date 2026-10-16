#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXX="clang++"
COMMON_FLAGS="-std=c++2c -freflection -freflection-latest -stdlib=libc++ -fPIC -shared -O2"
INCLUDES="-I/workspace -I/workspace/tests/bulk_transfer"
PYTHON_FLAGS=$(python3-config --includes)

echo "Compiling bulk_binding.cpp..."
$CXX $COMMON_FLAGS $INCLUDES $PYTHON_FLAGS bulk_binding.cpp -o bulk_test.so 2>&1

echo "Running bulk transfer tests..."
python3 test_bulk.py

# The threshold is a documented knob (-DMIRROR_BRIDGE_ARRAY_BORROW_BYTES), and
# 0 means "always borrow", which is the one setting that lets a zero-length
# vector reach PyMemoryView_FromMemory. data() is null for an empty vector and
# CPython asserts on a null buffer, so on a debug interpreter that is an abort.
# On a release interpreter it is benign -- frombytes guards its memcpy with
# n > 0 -- so this does not reproduce the abort here. It is a guard on a knob
# that is otherwise never exercised, and it would catch it on --with-pydebug.
# Built separately into its own directory because the threshold is a
# compile-time constant and the module keeps its name.
echo "Rebuilding with the borrow threshold at 0..."
ALT="$(mktemp -d)"
trap 'rm -rf "$ALT"' EXIT
$CXX $COMMON_FLAGS -DMIRROR_BRIDGE_ARRAY_BORROW_BYTES=0 $INCLUDES $PYTHON_FLAGS \
    bulk_binding.cpp -o "$ALT/bulk_test.so" 2>&1

ALT="$ALT" python3 - <<'PY'
import array, os, sys

# A fresh interpreter, so bulk_test resolves to the override build rather than
# the one beside this script.
sys.path.insert(0, os.environ["ALT"])
import bulk_test

assert os.path.dirname(bulk_test.__file__) == os.environ["ALT"], \
    f"imported the wrong build: {bulk_test.__file__}"

gen = bulk_test.DataGenerator()
gen.size = 0
for name, typecode in (("get_floats", "f"), ("get_doubles", "d"),
                       ("get_ints", "i"), ("get_shorts", "h")):
    result = getattr(gen, name)()
    assert isinstance(result, array.array), f"empty {name} gave {type(result)}"
    assert len(result) == 0 and result.typecode == typecode, (name, result)
print("  PASS: an empty vector with the threshold at 0 borrows nothing")

gen.size = 4
assert list(gen.get_ints()) == [0, 1, 4, 9], list(gen.get_ints())
print("  PASS: a 16-byte vector takes the borrow path and is correct")
PY
