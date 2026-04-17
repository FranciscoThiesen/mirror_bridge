#!/bin/bash
# Test inherited-method exposure end-to-end.
# mirror_bridge should walk class hierarchies via P2996 bases_of and expose
# every non-hidden inherited method, including overridden virtuals.

set -e
cd "$(dirname "$0")"

MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"

rm -rf build
"$MB_TOOL" generate src \
    --module inherit_mod \
    --lang python \
    --output build \
    --force 2>&1 | tail -10

python3 test_inheritance.py
