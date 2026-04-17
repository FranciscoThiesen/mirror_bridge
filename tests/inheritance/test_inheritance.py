"""Runtime test: inherited methods from non-virtual, virtual, and overridden
base-class methods are all exposed via auto-discovery + bases_of walking."""
import sys
sys.path.insert(0, "build")

import inherit_mod

fail = 0
def check(cond, msg):
    global fail
    if cond:
        print(f"  PASS: {msg}")
    else:
        print(f"  FAIL: {msg}")
        fail += 1

print("Testing inherited method exposure via P2996 bases_of:")
print()

# Classes should all be discovered and bound.
classes = sorted(c for c in dir(inherit_mod) if not c.startswith("_"))
check(set(classes) >= {"Geometry", "Geometry3D", "PointCloud", "TriangleMesh"},
      f"all four classes bound (got {classes})")

# PointCloud instance with methods inherited across 2 levels.
pc = inherit_mod.PointCloud()
pc_attrs = set(x for x in dir(pc) if not x.startswith("_"))
check("Size" in pc_attrs, "direct method Size exposed on PointCloud")
check("Dimension" in pc_attrs, "1-level inherited Dimension from Geometry3D")
check("GetName" in pc_attrs, "2-level inherited GetName from Geometry")
check("SetName" in pc_attrs, "2-level inherited SetName from Geometry")
check("GetCenter" in pc_attrs, "overridden virtual GetCenter exposed")
check("GetExtentFromCenter" in pc_attrs,
      "base-impl method that calls derived virtual")
check("GeometryType" in pc_attrs, "overridden GeometryType from Geometry")

# Inherited methods actually work at runtime.
pc.SetName("cloud_a")
check(pc.GetName() == "cloud_a", "SetName/GetName roundtrip (inherited)")

check(pc.Dimension() == 3, "Dimension returns 3 (inherited from Geometry3D)")

# Virtual dispatch: pc.GeometryType calls PointCloud's override (1), not
# Geometry's pure virtual which would be 0.
check(pc.GeometryType() == 1, f"virtual override: PointCloud.GeometryType = {pc.GeometryType()}")

# Eigen round-trip plus inherited method that calls derived virtual.
pc.points = [[1, 0, 0], [3, 0, 0], [0, 2, 0], [0, 0, 2]]
center = pc.GetCenter()
check(abs(center[0] - 1.0) < 1e-9, f"GetCenter Eigen: center_x={center[0]}")

# GetExtentFromCenter is defined in Geometry3D but calls GetCenter() which is
# virtual — polymorphic dispatch through inherited non-virtual must still
# reach PointCloud's override.
ext = pc.GetExtentFromCenter()
check(abs(ext[0] - 1.0) < 1e-9, "inherited method polymorphically calls derived virtual")

# TriangleMesh — same inheritance chain, different override.
tm = inherit_mod.TriangleMesh()
tm_attrs = set(x for x in dir(tm) if not x.startswith("_"))
check(tm_attrs >= {"Dimension", "GetName", "SetName", "GeometryType", "GetCenter",
                   "VertexCount", "TriangleCount"},
      "TriangleMesh has own methods + same inherited set")

check(tm.GeometryType() == 2,
      f"sibling override: TriangleMesh.GeometryType = {tm.GeometryType()} (not PointCloud's 1)")

tm.SetName("mesh_a")
check(tm.GetName() == "mesh_a", "shared inherited method on sibling type works")

# Abstract base classes should still be exposed (can't instantiate, but
# shared methods show up).
geom_attrs = set(x for x in dir(inherit_mod.Geometry) if not x.startswith("_"))
check("GetName" in geom_attrs and "SetName" in geom_attrs,
      "abstract Geometry exposes its own methods")

print()
if fail == 0:
    print("All inherited-method tests passed.")
    sys.exit(0)
else:
    print(f"{fail} test(s) FAILED")
    sys.exit(1)
