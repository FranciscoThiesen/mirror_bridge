"""Visual demo: uses the mirror_bridge binding (zero hand-written glue) to
generate, manipulate, and render 3D point clouds. Produces a PNG showing
the full pipeline: raw sphere + torus, after voxel downsampling, with
colored normals, and a Python-subclass override in action.

Every operation in this script crosses the C++/Python boundary through
mirror_bridge. No pybind11-style .def() anywhere — just the two bind_class
calls and two bind_function calls in binding.cpp."""
import sys
import os
import math
sys.path.insert(0, "build")

# matplotlib for rendering — it's just a 3D scatter visualization of the
# point clouds that mirror_bridge hands us from C++.
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa

import open3d_comprehensive as o3d

# -----------------------------------------------------------------------
# 1. Generate geometry in C++, access from Python
# -----------------------------------------------------------------------
sphere = o3d.create_sphere(1.0, 40)
torus = o3d.create_torus(2.2, 0.5, 60, 30)

# Shift the torus so the render shows both shapes distinctly.
shift = [4.0, 0.0, 0.0]
for i in range(len(torus.points)):
    torus.points[i] = [torus.points[i][0] + shift[0],
                       torus.points[i][1] + shift[1],
                       torus.points[i][2] + shift[2]]

print(f"sphere: {sphere.size()} points")
print(f"torus:  {torus.size()} points")

# -----------------------------------------------------------------------
# 2. Compute real stuff on the C++ side via the binding
# -----------------------------------------------------------------------
sphere_bbox = sphere.get_axis_aligned_bounding_box()
torus_bbox = torus.get_axis_aligned_bounding_box()
print(f"sphere AABB: {list(sphere_bbox.min_bound)} .. {list(sphere_bbox.max_bound)}")
print(f"torus  AABB: {list(torus_bbox.min_bound)} .. {list(torus_bbox.max_bound)}")

# Merge via operator+ — another reflection-derived feature.
merged = sphere + torus
print(f"merged: {merged.size()} points (operator+)")

# Estimate normals via kwargs + defaults (real reflection).
shaded = merged.estimate_normals(radius=0.05, max_nn=30)

# Voxel downsample via the C++ algorithm, 5cm voxels.
downsampled = shaded.voxel_down_sample(voxel_size=0.15)
print(f"downsampled: {downsampled.size()} points (voxel_size=0.15)")

# -----------------------------------------------------------------------
# 3. Python subclass with an overridden C++ virtual
# -----------------------------------------------------------------------
class TwistyPointCloud(o3d.PointCloud):
    """Override get_center() — demonstrates auto-trampoline.
    A Python-level override that C++ code internally calls reaches this."""
    def get_center(self):
        # Return a fake center to prove the trampoline routed here.
        return [99.0, 99.0, 99.0]

twisty = TwistyPointCloud()
twisty.points = [[0, 0, 0], [1, 1, 1]]
c = twisty.get_center()
print(f"TwistyPointCloud.get_center() = {list(c)} (Python override)")

# -----------------------------------------------------------------------
# 4. Render a figure proving it all works end-to-end
# -----------------------------------------------------------------------
def extract_xyz(pc):
    xs, ys, zs = [], [], []
    for p in pc.points:
        xs.append(p[0])
        ys.append(p[1])
        zs.append(p[2])
    return np.array(xs), np.array(ys), np.array(zs)

def extract_normals_colors(pc):
    """Color each point by the XY-plane projection of its normal vector."""
    cols = []
    for n in pc.normals:
        # Map (nx, ny, nz) ∈ [-1,1]³ to RGB ∈ [0,1]³.
        cols.append(((n[0] + 1) / 2, (n[1] + 1) / 2, (n[2] + 1) / 2))
    return np.array(cols)

fig = plt.figure(figsize=(16, 10), facecolor="white")
fig.suptitle("Real C++ point clouds, generated and processed via mirror_bridge",
             fontsize=15, fontweight="bold")

# Panel 1: sphere from create_sphere()
ax1 = fig.add_subplot(2, 3, 1, projection="3d")
xs, ys, zs = extract_xyz(sphere)
ax1.scatter(xs, ys, zs, c=zs, cmap="plasma", s=8, alpha=0.85)
ax1.set_title(f"o3d.create_sphere(radius=1)\n{sphere.size()} points",
              fontsize=10)
ax1.set_box_aspect([1, 1, 1])

# Panel 2: torus from create_torus()
ax2 = fig.add_subplot(2, 3, 2, projection="3d")
xs, ys, zs = extract_xyz(torus)
ax2.scatter(xs, ys, zs, c=zs, cmap="viridis", s=8, alpha=0.85)
ax2.set_title(f"o3d.create_torus(outer=2.2, inner=0.5)\n{torus.size()} points",
              fontsize=10)
ax2.set_box_aspect([1, 1, 1])

# Panel 3: merged via operator+
ax3 = fig.add_subplot(2, 3, 3, projection="3d")
xs, ys, zs = extract_xyz(merged)
ax3.scatter(xs, ys, zs, c=xs, cmap="coolwarm", s=5, alpha=0.6)
ax3.set_title(f"merged = sphere + torus\n(operator+, {merged.size()} points)",
              fontsize=10)
ax3.set_box_aspect([1, 1, 1])

# Panel 4: normals colored
ax4 = fig.add_subplot(2, 3, 4, projection="3d")
xs, ys, zs = extract_xyz(shaded)
cols = extract_normals_colors(shaded)
ax4.scatter(xs, ys, zs, c=cols, s=7, alpha=0.9)
ax4.set_title(f"estimate_normals(radius=.., max_nn=..)\nRGB = normal direction",
              fontsize=10)
ax4.set_box_aspect([1, 1, 1])

# Panel 5: voxel downsampled
ax5 = fig.add_subplot(2, 3, 5, projection="3d")
xs, ys, zs = extract_xyz(downsampled)
ax5.scatter(xs, ys, zs, c=zs, cmap="turbo", s=30)
ax5.set_title(f"voxel_down_sample(voxel_size=0.15)\n{downsampled.size()} points (from {merged.size()})",
              fontsize=10)
ax5.set_box_aspect([1, 1, 1])

# Panel 6: bounding boxes
ax6 = fig.add_subplot(2, 3, 6, projection="3d")
xs, ys, zs = extract_xyz(merged)
ax6.scatter(xs, ys, zs, c="lightgray", s=3, alpha=0.3)
def draw_bbox(ax, bbox, color):
    lo = bbox.min_bound
    hi = bbox.max_bound
    # 12 edges of the box
    corners = [
        [lo[0], lo[1], lo[2]], [hi[0], lo[1], lo[2]],
        [hi[0], hi[1], lo[2]], [lo[0], hi[1], lo[2]],
        [lo[0], lo[1], hi[2]], [hi[0], lo[1], hi[2]],
        [hi[0], hi[1], hi[2]], [lo[0], hi[1], hi[2]],
    ]
    edges = [(0,1),(1,2),(2,3),(3,0),
             (4,5),(5,6),(6,7),(7,4),
             (0,4),(1,5),(2,6),(3,7)]
    for i, j in edges:
        ax.plot([corners[i][0], corners[j][0]],
                [corners[i][1], corners[j][1]],
                [corners[i][2], corners[j][2]],
                color=color, linewidth=2)

draw_bbox(ax6, sphere_bbox, "crimson")
draw_bbox(ax6, torus_bbox, "royalblue")
ax6.set_title("get_axis_aligned_bounding_box()\n(polymorphic return)",
              fontsize=10)
ax6.set_box_aspect([1, 1, 1])

plt.tight_layout()

# Web-sized PNG for the blog — 80 DPI gives crisp text on retina screens
# while staying cheap to download. Pillow post-pass trims the file further
# (matplotlib's PNG writer leaves some metadata room).
out_png = "mirror_bridge_open3d_demo.png"
plt.savefig(out_png, dpi=80, bbox_inches="tight", facecolor="white")

from PIL import Image
img = Image.open(out_png)
img.save(out_png, "PNG", optimize=True)
print(f"\nRendered to {out_png}")
print(f"  Image: {os.path.getsize(out_png) // 1024} KB")
