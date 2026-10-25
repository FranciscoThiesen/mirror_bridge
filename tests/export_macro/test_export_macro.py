#!/usr/bin/env python3
"""The module a header full of export macros produces.

Built by build_and_test.sh, which runs `mirror_bridge generate` over
export_macro/src."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "build"))

import exportlib_py  # noqa: E402

w = exportlib_py.Widget()
assert w.width == 3, w.width
assert w.height == 4, w.height
assert w.area() == 12, w.area()

h = exportlib_py.Handle()
assert h.id == 7, h.id
assert h.doubled() == 14, h.doubled()

lst = exportlib_py.WidgetList()
assert lst.count == 2, lst.count
assert lst.twice() == 4, lst.twice()

cur = exportlib_py.WidgetCursor()
assert cur.next() == 1
assert cur.index == 1

wide = exportlib_py.Wide()
assert wide.sum() == 4.0, wide.sum()

tagged = exportlib_py.Tagged()
assert tagged.tagged() == 22, tagged.tagged()

sealed = exportlib_py.Sealed()
assert sealed.next_serial() == 6, sealed.next_serial()

for absent in ("EXPORTLIB_API", "Internal", "Secret"):
    assert not hasattr(exportlib_py, absent), f"{absent} should not be in the module"

# An enum beside the classes is bound too, as an enum.IntEnum.
assert hasattr(exportlib_py, "Status"), "the enum should be in the module"
assert [m.name for m in exportlib_py.Status] == ["Idle", "Busy"], list(exportlib_py.Status)
assert exportlib_py.Status.Busy == 1

print("✓ export-macro discovery test passed")
