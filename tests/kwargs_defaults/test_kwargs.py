"""Keyword arguments and default parameter values, both derived from
reflection (has_default_argument + identifier_of on parameters). No hand-
written binding annotations needed."""
import sys
sys.path.append("build")

import kwargs_defaults_test as mod

fail = 0
def check(cond, msg):
    global fail
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if not cond: fail += 1

pc = mod.PointCloud()

print("Defaults are applied when omitted:")
check(pc.select_by_index(42) == "42", "select_by_index(42) uses invert=false default")
check(pc.estimate_normals() == 30, "estimate_normals() uses all defaults")
check(pc.estimate_normals(0.2) == 30, "estimate_normals(0.2) uses remaining defaults")
check(pc.estimate_normals(0.2, 50) == 50, "estimate_normals(radius, max_nn) uses one default")

print()
print("Positional args explicitly override defaults:")
check(pc.select_by_index(3, True) == "invert_3", "select_by_index(3, True)")
check(pc.estimate_normals(0.1, 30, False) == 40,
      f"estimate_normals(0.1, 30, False) = {pc.estimate_normals(0.1, 30, False)}")

print()
print("Keyword arguments, resolved by reflection-provided names:")
check(pc.select_by_index(index=7) == "7", "kwarg only")
check(pc.select_by_index(index=5, invert=True) == "invert_5", "both kwargs")
check(pc.select_by_index(invert=True, index=9) == "invert_9",
      "kwargs in reversed order")

print()
print("Required arg must be provided:")
try:
    pc.voxel_down_sample()
    check(False, "voxel_down_sample() without required arg should fail")
except TypeError:
    check(True, "missing required arg correctly rejected")

try:
    pc.voxel_down_sample(nonexistent=1.0)
    check(False, "unknown keyword should fail")
except TypeError:
    check(True, "unknown keyword correctly rejected")

pc2 = pc.voxel_down_sample(voxel_size=0.05)
check(abs(pc2.last_voxel_size_ - 0.05) < 1e-9, "voxel_down_sample with kwarg returns result")

print()
if fail == 0:
    print("Keyword-argument and default-argument tests passed.")
    sys.exit(0)
else:
    print(f"{fail} FAILED")
    sys.exit(1)
