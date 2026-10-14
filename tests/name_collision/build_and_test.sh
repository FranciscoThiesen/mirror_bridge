#!/bin/bash
# Two classes that want the same unqualified name.
#
# A Python module exposes every class under its simple name, so only one of
# them can answer to it. That used to be whichever was bound last, with no
# effect on the exit status and no effect on the banner:
#
#     ⚠ 'Config' is the binding name of 2 classes
#     ✓ Successfully built bindings for: python
#     >>> dup.Config().marker()
#     999
#
# The public class was unreachable, the implementation detail had taken its
# name, and a warning had been printed. A warning does not make a wrong module
# right.
set -e
cd "$(dirname "$0")"

MB_TOOL="$(cd ../.. && pwd)/tools/mirror_bridge"

rm -rf build
out=$("$MB_TOOL" generate src --module collide_py --lang python --output build \
        --force --keep-generated 2>&1 | sed -e 's/\x1b\[[0-9;]*m//g')
echo "$out" | tail -20

fail=0
expect() { if [[ "$2" == *"$3"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (expected: $3)"; fail=1; fi; }
reject() { if [[ "$2" != *"$3"* ]]; then echo "  ✓ $1"; else echo "  ✗ $1 (unexpected: $3)"; fail=1; fi; }

echo ""
expect "the collision is named with both classes"  "$out" "'Config' is the binding name of 2 classes"
expect "and says which one takes the name"         "$out" "lib::Config is bound"
expect "the loser is reported by name"             "$out" "lib::detail::Config"
expect "under a heading that counts it"            "$out" "lost their unqualified name"

binding=build/collide_py_python_binding.cpp
expect "only the shallower class is bound"  "$(cat $binding)" 'bind_class_when_bindable<lib::Config>(m, "Config")'
reject "the deeper one is not emitted"      "$(cat $binding)" 'lib::detail::Config>(m, "Config")'

echo ""
echo "The public class is the one that answers:"
PYTHONPATH=build python3 -c "
import collide_py as c
cfg = c.Config()
assert cfg.timeout == 30, cfg.timeout
assert cfg.doubled() == 60, cfg.doubled()
assert not hasattr(cfg, 'marker'), 'the detail class took the name'
assert not hasattr(cfg, 'scratch'), 'the detail class took the name'
# A class with no collision is untouched.
s = c.Session()
assert s.next() == 6, s.next()
print('  ✓ Config.doubled() == 60, and Session still binds')
"

echo ""
[ "$fail" -eq 0 ] && echo "✓ name-collision test passed"
[ "$fail" -eq 0 ]
