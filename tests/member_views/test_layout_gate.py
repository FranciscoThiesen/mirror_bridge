#!/usr/bin/env python3
"""A module built against a different wrapper layout says so at import.

The layout tag in every cross-module key is what stops two mirror_bridge
versions from sharing a PyTypeObject whose tp_basicsize they disagree about.
The safe outcome is that neither recognises the other's objects, but the
conversion failure that results names a class whose name looks right, so the
cause has to be reported where it can be understood.

This has to run before any bound module is imported, which is why it is its
own file: the registry is seeded first, so the import sees a foreign layout.
"""

import sys
import os
import warnings

# Seed the cross-module registry as a module built against another layout
# would have left it. get_python_named_registry uses whatever dict is already
# in sys.modules under this name.
sys.modules['_mirror_bridge_types'] = {'__mirror_bridge_layout__': '#mbabi_other'}

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))

print("Test 1: importing against a foreign layout warns...")
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    import member_views  # noqa: F401
    texts = [str(w.message) for w in caught if w.category is RuntimeWarning]

assert any("wrapper layout" in t for t in texts), texts
assert any("#mbabi_other" in t for t in texts), texts
# The message has to tell the reader what to do, not just that something is off.
assert any("Rebuild" in t for t in texts), texts
print("  ✓ RuntimeWarning names both layouts and says to rebuild")

print("Test 2: it is said once, not once per bound class...")
# member_views binds four classes; a warning per class would bury the signal.
layout_warnings = [t for t in texts if "wrapper layout" in t]
assert len(layout_warnings) == 1, layout_warnings
print("  ✓ exactly one warning for the module")

print("Test 3: the module still works...")
# A layout clash must not stop the module that reports it from functioning;
# only passing objects between the two is refused.
o = member_views.Owner()
o.plain.x = 4.0
assert o.plain.x == 4.0
print("  ✓ import completed and the module behaves normally")

print("\nAll layout gate tests passed!")
