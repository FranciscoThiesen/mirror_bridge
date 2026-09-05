#!/bin/bash
# Template instantiation end-to-end: `mirror_bridge generate` plans the
# instantiations (discover/probe rounds), builds the module with stubs, and
# the Python test exercises families, CTAD-like construction, member
# templates, function families and the skip reporting.
set -e
cd "$(dirname "$0")"

MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"

rm -rf build
"$MB_TOOL" generate src \
    --module geom \
    --lang python \
    --output build \
    --stubs \
    --keep-generated \
    --instantiate "geom::Stack<float>" \
    --instantiate "geom::twice<short>" \
    --force 2>&1 | tail -15

# The plan report is part of the contract: users read it to see what was
# bound and why something was not.
grep -q "geom::Vector3<float>" build/geom_plan.txt
grep -q "geom::labels" build/geom_plan.txt
grep -q "overloaded" build/geom_plan.txt
grep -q "geom::deref<long>.*raw pointer" build/geom_plan.txt

python3 test_template_instantiation.py
