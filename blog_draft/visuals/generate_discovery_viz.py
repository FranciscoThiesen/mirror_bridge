"""Generate an auto-discovery visualization for the blog.

Shows how mirror_bridge's brace-depth parser walks C++ headers and
emits bind_class<T> calls for every discovered type, including nested
ones that need qualification.

Three panels side-by-side:
  1. Slice of two real Open3D headers with discovered classes marked.
  2. Brace-depth vs. line number with a dot at each discovery event.
  3. The bind_class<T> output the parser emits, qualified names and
     all.

Outputs `auto_discovery_traversal.png` (trimmed for blog use).
"""
from __future__ import annotations

import os

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np

# ---- Color palette --------------------------------------------------------
BG         = "#0f172a"   # slate-900
PANEL      = "#1e293b"   # slate-800
INK        = "#e2e8f0"   # slate-200
DIM        = "#94a3b8"   # slate-400
ACCENT1    = "#22d3ee"   # cyan-400   (discovered)
ACCENT2    = "#a78bfa"   # violet-400 (nested discovery)
ACCENT3    = "#34d399"   # emerald-400 (output line)
GRID       = "#334155"   # slate-700

HEADER_FONT = {"family": "monospace", "size": 10, "color": INK}
TITLE_FONT  = {"size": 14, "weight": "bold", "color": INK}
SUBT_FONT   = {"size": 10, "color": DIM}

# ---- Source content to visualize ------------------------------------------
# Two compact, lightly-edited header slices. We keep these short so the
# visual stays readable; real Open3D headers have the same shape.

HEADER_A = [
    ("// HalfEdgeTriangleMesh.h", None,    None),
    ("class HalfEdgeTriangleMesh",   1, "HalfEdgeTriangleMesh"),
    ("        : public MeshBase {",  1, None),
    ("public:",                       1, None),
    ("    struct HalfEdge {",         2, "HalfEdgeTriangleMesh::HalfEdge"),
    ("        int vertex_indices[2];",2, None),
    ("        int triangle_index;",   2, None),
    ("    };",                        1, None),
    ("    std::vector<HalfEdge> es;", 1, None),
    ("};",                            0, None),
]

HEADER_B = [
    ("// KDTreeSearchParam.h",       None, None),
    ("class KDTreeSearchParam {",    1, "KDTreeSearchParam"),
    ("public:",                      1, None),
    ("    enum class SearchType {",  2, "KDTreeSearchParam::SearchType"),
    ("        Knn = 0,",             2, None),
    ("        Radius = 1,",          2, None),
    ("    };",                       1, None),
    ("    SearchType type_;",        1, None),
    ("};",                           0, None),
]

BIND_LINES = [
    ("bind_class<HalfEdgeTriangleMesh>(m, \"HalfEdgeTriangleMesh\");", False),
    ("bind_class<HalfEdgeTriangleMesh::HalfEdge>(m, \"HalfEdge\");",    True),
    ("bind_class<KDTreeSearchParam>(m, \"KDTreeSearchParam\");",        False),
    ("bind_class<KDTreeSearchParam::SearchType>(m, \"SearchType\");",   True),
    ("// ... 43 more bind_class<T> lines ...",                          False),
]

# ---- Figure setup ---------------------------------------------------------
fig = plt.figure(figsize=(15.5, 7.2), facecolor=BG)
fig.suptitle(
    "mirror_bridge auto-discovery: brace-depth walk → fully-qualified bind_class<T>",
    fontsize=15, fontweight="bold", color=INK, y=0.97,
)

# Three unequal columns: source (wide), depth trace (wide), output (wider).
gs = fig.add_gridspec(1, 3, width_ratios=[1.05, 1.0, 1.15],
                      left=0.035, right=0.98, top=0.89, bottom=0.06, wspace=0.12)

# ---- Panel 1: headers with discovered classes -----------------------------
ax1 = fig.add_subplot(gs[0, 0], facecolor=PANEL)
ax1.set_xlim(0, 1)
ax1.set_ylim(0, 1)
ax1.set_xticks([]); ax1.set_yticks([])
for s in ax1.spines.values():
    s.set_color(GRID); s.set_linewidth(0.8)
ax1.set_title("1. Input: Open3D headers", fontsize=12, color=INK,
              loc="left", pad=10)

def draw_header(ax, rows, y_top):
    line_h = 0.042
    y = y_top
    for code, depth, discovered in rows:
        color = DIM if code.startswith("//") else INK
        ax.text(0.04, y, code, **{**HEADER_FONT, "color": color},
                va="top", ha="left")
        if discovered is not None:
            marker_color = ACCENT2 if "::" in discovered else ACCENT1
            ax.plot(0.955, y - 0.012, "o", markersize=8,
                    color=marker_color, mec=marker_color)
            ax.text(0.92, y, f"d={depth}", fontsize=8,
                    color=marker_color, va="top", ha="right",
                    family="monospace")
        y -= line_h
    return y

y_end = draw_header(ax1, HEADER_A, 0.95)
# separator line
ax1.axhline(y_end - 0.01, color=GRID, linewidth=0.8, xmin=0.03, xmax=0.97)
draw_header(ax1, HEADER_B, y_end - 0.035)

# legend inside panel 1
legend_elements = [
    Line2D([0], [0], marker="o", color=PANEL, markerfacecolor=ACCENT1,
           markersize=8, label="top-level class"),
    Line2D([0], [0], marker="o", color=PANEL, markerfacecolor=ACCENT2,
           markersize=8, label="nested class (depth ≥ 2)"),
]
ax1.legend(handles=legend_elements, loc="lower left",
           bbox_to_anchor=(0.02, 0.015), frameon=False,
           labelcolor=INK, fontsize=8.5,
           handletextpad=0.6, handlelength=1.2, borderaxespad=0)

# ---- Panel 2: brace-depth trace -------------------------------------------
ax2 = fig.add_subplot(gs[0, 1], facecolor=PANEL)
ax2.set_title("2. Brace-depth parser trace", fontsize=12, color=INK,
              loc="left", pad=10)

# Build a synthetic but honest brace-depth curve across the two headers.
# Each step is a token; we just care about the shape.
depth_a = [0, 0, 1, 1, 1, 2, 2, 2, 1, 1, 0]
depth_b = [0, 1, 1, 2, 2, 2, 1, 1, 0]
depth_full = depth_a + [0] + depth_b   # single token gap between files
x_full = np.arange(len(depth_full))

ax2.plot(x_full, depth_full, color=ACCENT1, linewidth=2.4, drawstyle="steps-post")
ax2.fill_between(x_full, 0, depth_full, step="post",
                 color=ACCENT1, alpha=0.14)

# Discovery markers at class openings (lines where depth increased to 1 or 2
# while a "class/struct/enum" keyword was seen on that token).
# (positions derived from the lists above)
disc_points = [
    (1,  1, "HalfEdgeTriangleMesh",              ACCENT1),
    (4,  2, "HalfEdgeTriangleMesh::HalfEdge",    ACCENT2),
    (12, 1, "KDTreeSearchParam",                 ACCENT1),
    (14, 2, "KDTreeSearchParam::SearchType",     ACCENT2),
]
for x, d, name, c in disc_points:
    ax2.plot(x, d, "o", markersize=11, color=c, mec=INK, mew=1.2, zorder=5)

# Axes cosmetics
ax2.set_xlim(-0.5, len(depth_full) - 0.5)
ax2.set_ylim(-0.3, 2.8)
ax2.set_yticks([0, 1, 2])
ax2.set_yticklabels(["0", "1", "2"], color=INK)
ax2.set_xticks([])
ax2.set_xlabel("token stream →", color=DIM, fontsize=10, labelpad=6)
# y-axis label omitted; the tick values (0, 1, 2) plus the panel title
# "Brace-depth parser trace" make the axis meaning obvious and free up the
# horizontal gap for the between-panel flow arrow.
ax2.tick_params(colors=INK)
ax2.grid(True, axis="y", color=GRID, linewidth=0.6, alpha=0.6)
for s in ax2.spines.values():
    s.set_color(GRID); s.set_linewidth(0.8)

# File-boundary marker
boundary_x = len(depth_a) + 0.5
ax2.axvline(boundary_x, color=DIM, linestyle=":", linewidth=0.9)
ax2.text(boundary_x, 2.55, "next file", color=DIM, fontsize=8,
         ha="center", va="bottom", style="italic")

# Annotation callouts
ax2.annotate("depth=2 when\nclass detected here →\nqualify as Parent::Child",
             xy=(4, 2), xytext=(4.8, 2.45),
             fontsize=8.5, color=ACCENT2, ha="left",
             arrowprops=dict(arrowstyle="-", color=ACCENT2, lw=0.9))

# ---- Panel 3: emitted bind_class<T> lines ---------------------------------
ax3 = fig.add_subplot(gs[0, 2], facecolor=PANEL)
ax3.set_xlim(0, 1); ax3.set_ylim(0, 1)
ax3.set_xticks([]); ax3.set_yticks([])
ax3.set_title("3. Output: bind_class<T> per discovery",
              fontsize=12, color=INK, loc="left", pad=10)
for s in ax3.spines.values():
    s.set_color(GRID); s.set_linewidth(0.8)

ax3.text(0.04, 0.93, "MIRROR_BRIDGE_MODULE(open3d_full,",
         family="monospace", fontsize=10, color=DIM, va="top")

y = 0.84
for line, nested in BIND_LINES:
    color = ACCENT2 if nested else (DIM if line.startswith("//") else ACCENT3)
    ax3.text(0.06, y, "    " + line,
             family="monospace", fontsize=9.6, color=color, va="top")
    y -= 0.058

ax3.text(0.04, y - 0.005, ")", family="monospace",
         fontsize=10, color=DIM, va="top")

# Summary stats footer, pulled up right under the output.
footer_top = y - 0.08
ax3.axhline(footer_top + 0.02, color=GRID, linewidth=0.8,
            xmin=0.04, xmax=0.96)
ax3.text(0.06, footer_top - 0.02,
         "47 classes discovered from 38 header files",
         fontsize=10.5, color=INK, fontweight="bold", va="top",
         family="monospace")
ax3.text(0.06, footer_top - 0.085,
         "0 lines of hand-written binding code",
         fontsize=10.5, color=ACCENT1, fontweight="bold", va="top",
         family="monospace")
ax3.text(0.06, footer_top - 0.15,
         "$ mirror_bridge generate …",
         fontsize=9.5, color=DIM, va="top", family="monospace")

# ---- Between-panel flow arrows --------------------------------------------
# Compute arrow positions from the actual axes bounding boxes so they
# never overlap the panels, regardless of layout tweaks.
fig.canvas.draw()
p1 = ax1.get_position(); p2 = ax2.get_position(); p3 = ax3.get_position()

import matplotlib.patches as _mp

def gap_arrow(x0, x1):
    y = (p1.y0 + p1.y1) / 2
    fig.patches.append(_mp.FancyArrowPatch(
        (x0, y), (x1, y),
        arrowstyle="-|>", mutation_scale=22,
        color=ACCENT1, lw=2.2, transform=fig.transFigure,
        shrinkA=0, shrinkB=0, alpha=0.9,
    ))

gap_arrow(p1.x1 + 0.004, p2.x0 - 0.004)
gap_arrow(p2.x1 + 0.004, p3.x0 - 0.004)

# ---- Save -----------------------------------------------------------------
out_dir = os.path.dirname(os.path.abspath(__file__))
out_path = os.path.join(out_dir, "auto_discovery_traversal.png")
fig.savefig(out_path, dpi=90, facecolor=BG, bbox_inches="tight",
            pad_inches=0.15)

try:
    from PIL import Image
    Image.open(out_path).save(out_path, "PNG", optimize=True)
except ImportError:
    pass

print(f"Rendered {out_path}")
print(f"  Size: {os.path.getsize(out_path) // 1024} KB")
