"""Auto-discovery visualization: proper tree + DFS traversal path.

Shows the parse as what it actually is: a depth-first traversal of the
(file → class → nested-class) tree, with each discovery numbered in
visit order and an explicit arrow path drawn through the numbers.

Layout, left to right:
   col A   header-file nodes (roots of each per-file subtree)
   col B   top-level class nodes (depth 1)
   col C   nested class nodes (depth 2)
   col D   emitted bind_class<T> line for each visited node

A highlighted dashed path visits every discovered class in DFS
pre-order. Numbered badges on each node show the visit order.

Outputs `auto_discovery_traversal.png`.
"""
from __future__ import annotations

import os

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
import matplotlib.patches as mp
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle
from matplotlib.path import Path as MPath

# ---- Palette ----------------------------------------------------------
BG        = "#0B1220"
PANEL     = "#151E33"
PANEL_HI  = "#1C2640"
INK       = "#F1F5F9"
DIM       = "#94A3B8"
MUTED     = "#64748B"
GRID      = "#2A3752"

FILE_COL   = "#60A5FA"   # blue  (file nodes)
TOP_COL    = "#34D399"   # green (top-level class)
NESTED_COL = "#F472B6"   # pink  (nested class)
PATH_COL   = "#FBBF24"   # amber (DFS path)
OUT_COL    = "#A5B4FC"   # indigo (emit line)

MONO = "DejaVu Sans Mono"

# ---- The tree we're going to draw -------------------------------------
# Each file has one or more top-level classes. Each top-level class may
# have one or more nested classes. This is a realistic slice of Open3D.
TREE = [
    {
        "file": "BoundingVolume.h",
        "classes": [
            {"name": "OrientedBoundingBox",     "nested": []},
            {"name": "AxisAlignedBoundingBox",  "nested": []},
        ],
    },
    {
        "file": "TriangleMesh.h",
        "classes": [
            {"name": "TriangleMesh", "nested": []},
            {"name": "Material",     "nested": ["MaterialParameter"]},
        ],
    },
    {
        "file": "HalfEdgeTriangleMesh.h",
        "classes": [
            {"name": "HalfEdgeTriangleMesh", "nested": ["HalfEdge"]},
        ],
    },
    {
        "file": "KDTreeSearchParam.h",
        "classes": [
            {"name": "KDTreeSearchParam", "nested": ["SearchType"]},
        ],
    },
]

# ---- Flatten to DFS pre-order visit list ------------------------------
# Each visit gets a number, a depth (1 or 2), a qualified name, and
# an index back to its file.
class Visit:
    __slots__ = ("idx", "name", "qualname", "depth", "file", "parent",
                 "x", "y")

    def __init__(self, idx, name, qualname, depth, file, parent):
        self.idx = idx
        self.name = name
        self.qualname = qualname
        self.depth = depth
        self.file = file
        self.parent = parent
        self.x = 0.0
        self.y = 0.0

visits: list[Visit] = []
for f in TREE:
    for c in f["classes"]:
        idx = len(visits) + 1
        v_top = Visit(idx, c["name"], c["name"], 1, f["file"], None)
        visits.append(v_top)
        for n in c["nested"]:
            idx2 = len(visits) + 1
            qual = f"{c['name']}::{n}"
            visits.append(Visit(idx2, n, qual, 2, f["file"], v_top))

TOTAL_DISCOVERED = 47
TOTAL_FILES      = 38

# ---- Figure -----------------------------------------------------------
fig = plt.figure(figsize=(16, 9.2), facecolor=BG)

outer = fig.add_gridspec(
    2, 1, height_ratios=[0.11, 0.89],
    left=0.02, right=0.98, top=0.975, bottom=0.03,
    hspace=0.01,
)

# ---- HEADER -----------------------------------------------------------
axH = fig.add_subplot(outer[0, 0])
axH.set_facecolor(BG)
axH.set_xlim(0, 1); axH.set_ylim(0, 1)
axH.set_xticks([]); axH.set_yticks([])
for s in axH.spines.values():
    s.set_visible(False)

axH.text(0.005, 0.76, "Auto-discovery",
         fontsize=22, color=INK, weight="bold", family=MONO,
         va="center")
axH.text(0.005, 0.28,
         "DFS walk of the (file → class → nested) tree"
         "  ·  one bind_class<T> per discovery  ·  zero glue",
         fontsize=12, color=DIM, style="italic", family=MONO,
         va="center")

def stat(ax, x_right, num, label, color):
    ax.text(x_right, 0.74, num, fontsize=26, color=color,
            weight="bold", va="center", ha="right", family=MONO)
    ax.text(x_right + 0.006, 0.74, label, fontsize=11, color=DIM,
            va="center", ha="left", family=MONO)

stat(axH, 0.47, str(TOTAL_DISCOVERED), "  classes discovered",    TOP_COL)
stat(axH, 0.71, str(TOTAL_FILES),      "  headers scanned",       FILE_COL)
stat(axH, 0.92, "0",                   "  lines of binding code", NESTED_COL)

axH.plot([0.005, 0.995], [0.02, 0.02], color=GRID, lw=0.8)

# ---- MAIN TREE PANEL --------------------------------------------------
axT = fig.add_subplot(outer[1, 0])
axT.set_facecolor(BG)
axT.set_xlim(0, 1); axT.set_ylim(0, 1)
axT.set_xticks([]); axT.set_yticks([])
for s in axT.spines.values():
    s.set_visible(False)

# Column x-anchors
X_FILE  = 0.08
X_TOP   = 0.30
X_NEST  = 0.54
X_OUT   = 0.78
X_OUT_END = 0.98

# Column header labels
def col_label(ax, x, text, color=DIM):
    ax.text(x, 0.965, text, fontsize=10, color=color, weight="bold",
            family=MONO, va="top", ha="left")

col_label(axT, X_FILE,  "HEADERS",                FILE_COL)
col_label(axT, X_TOP,   "TOP-LEVEL  (depth 1)",   TOP_COL)
col_label(axT, X_NEST,  "NESTED  (depth 2)",      NESTED_COL)
col_label(axT, X_OUT,   "EMITTED",                OUT_COL)

# Sub-label underneath the columns
axT.text(X_FILE, 0.932, "38 files scanned, 4 shown",
         fontsize=8.5, color=MUTED, style="italic", family=MONO, va="top")
axT.text(X_TOP, 0.932, "DFS visit order shown in badge",
         fontsize=8.5, color=MUTED, style="italic", family=MONO, va="top")

# ---- Compute vertical positions --------------------------------------
# Each visit (row) gets a y slot. Uniform spacing, oldest-first on top.
Y_TOP   = 0.88
Y_BOT   = 0.15
row_gap = (Y_TOP - Y_BOT) / max(len(visits) - 1, 1)

for i, v in enumerate(visits):
    v.y = Y_TOP - i * row_gap
    v.x = X_NEST if v.depth == 2 else X_TOP

# File y = mean y of its classes
file_y: dict[str, float] = {}
file_class_count: dict[str, int] = {}
for v in visits:
    file_y.setdefault(v.file, 0.0)
    file_y[v.file] += v.y
    file_class_count[v.file] = file_class_count.get(v.file, 0) + 1
for k in file_y:
    file_y[k] /= file_class_count[k]

# ---- Draw the edges first (behind the nodes) -------------------------
def draw_edge(ax, x0, y0, x1, y1, color, lw=1.3, alpha=0.55, style="-"):
    """Right-angle routed edge: horizontal then vertical then horizontal."""
    mid_x = (x0 + x1) / 2
    verts = [(x0, y0), (mid_x, y0), (mid_x, y1), (x1, y1)]
    codes = [MPath.MOVETO, MPath.LINETO, MPath.LINETO, MPath.LINETO]
    path = MPath(verts, codes)
    patch = mp.PathPatch(path, fill=False, edgecolor=color,
                         linewidth=lw, alpha=alpha, linestyle=style)
    ax.add_patch(patch)

# Edges: file → each of its top-level classes
NODE_W_FILE = 0.20
NODE_W_CLS  = 0.22
NODE_H      = 0.055

for file, fy in file_y.items():
    for v in visits:
        if v.file == file and v.depth == 1:
            x0 = X_FILE + NODE_W_FILE
            x1 = v.x
            draw_edge(axT, x0, fy, x1, v.y, FILE_COL, lw=1.4, alpha=0.55)

# Edges: top-level → nested
for v in visits:
    if v.depth == 2 and v.parent is not None:
        p = v.parent
        x0 = p.x + NODE_W_CLS
        x1 = v.x
        draw_edge(axT, x0, p.y, x1, v.y, TOP_COL, lw=1.4, alpha=0.55)

# Edges: class → its emit line (subtle dashed)
for v in visits:
    x0 = v.x + NODE_W_CLS
    y0 = v.y
    x1 = X_OUT - 0.01
    y1 = v.y
    draw_edge(axT, x0, y0, x1, y1, OUT_COL, lw=0.8, alpha=0.35,
              style=":")

# ---- Draw file-nodes (rounded rectangles) ----------------------------
def rounded_box(ax, x, y_center, w, h, fill, edge, lw=1.5, rounding=0.012):
    box = FancyBboxPatch(
        (x, y_center - h / 2), w, h,
        boxstyle=f"round,pad=0,rounding_size={rounding}",
        linewidth=lw, edgecolor=edge, facecolor=fill,
    )
    ax.add_patch(box)

for file, fy in file_y.items():
    rounded_box(axT, X_FILE, fy, NODE_W_FILE, NODE_H,
                fill=PANEL, edge=FILE_COL, lw=1.6)
    # left accent strip
    strip = mp.Rectangle(
        (X_FILE, fy - NODE_H / 2), 0.006, NODE_H,
        facecolor=FILE_COL, edgecolor="none",
    )
    axT.add_patch(strip)
    axT.text(X_FILE + 0.022, fy + 0.001, file,
             fontsize=10.5, color=INK, family=MONO, weight="bold",
             va="center", ha="left")

# "... more files" placeholder below last file
more_y = min(file_y.values()) - 0.075
axT.text(X_FILE + NODE_W_FILE / 2, more_y,
         "· · · 34 more files · · ·",
         fontsize=9.5, color=MUTED, family=MONO, style="italic",
         va="center", ha="center")

# ---- Draw class nodes ------------------------------------------------
def class_node(ax, v: Visit):
    color = TOP_COL if v.depth == 1 else NESTED_COL
    w = NODE_W_CLS
    # Node box
    rounded_box(ax, v.x, v.y, w, NODE_H,
                fill=PANEL_HI, edge=color, lw=1.8)
    # Class name
    name = v.name if len(v.name) <= 22 else v.name[:20] + "…"
    ax.text(v.x + 0.050, v.y + 0.001, name,
            fontsize=10.3, color=INK, family=MONO, weight="bold",
            va="center", ha="left")
    # Depth label small
    ax.text(v.x + w - 0.012, v.y + 0.001,
            f"d={v.depth}",
            fontsize=8.5, color=color, family=MONO,
            va="center", ha="right", weight="bold")
    # Numbered badge on the left end of the node
    badge_cx = v.x + 0.022
    badge_cy = v.y
    circ = Circle((badge_cx, badge_cy), 0.016,
                  facecolor=PATH_COL, edgecolor=BG, linewidth=1.8,
                  zorder=4)
    ax.add_patch(circ)
    ax.text(badge_cx, badge_cy, str(v.idx),
            fontsize=10, color=BG, family=MONO, weight="bold",
            va="center", ha="center", zorder=5)

for v in visits:
    class_node(axT, v)

# ---- Draw the DFS traversal path -------------------------------------
# Path goes from badge center of visit[i] → badge center of visit[i+1].
# Style: thick dashed amber arrow segments for each hop.
for i in range(len(visits) - 1):
    a = visits[i]; b = visits[i + 1]
    x0, y0 = a.x + 0.022, a.y
    x1, y1 = b.x + 0.022, b.y
    # Offset start/end to the badge edge so the arrow isn't buried.
    arrow = FancyArrowPatch(
        (x0, y0), (x1, y1),
        connectionstyle="arc3,rad=-0.25",
        arrowstyle="-|>", mutation_scale=18,
        color=PATH_COL, lw=1.8, linestyle=(0, (4, 3)),
        alpha=0.85, shrinkA=14, shrinkB=14,
        zorder=3,
    )
    axT.add_patch(arrow)

# Callout labeling the path
path_label_x = 0.50
path_label_y = 0.075
box = FancyBboxPatch(
    (path_label_x - 0.12, path_label_y - 0.024), 0.24, 0.050,
    boxstyle="round,pad=0,rounding_size=0.014",
    linewidth=1.2, edgecolor=PATH_COL, facecolor=PANEL,
)
axT.add_patch(box)
axT.text(path_label_x, path_label_y, "DFS pre-order: 1 → 2 → 3 → … → 9",
         fontsize=10, color=PATH_COL, family=MONO, weight="bold",
         va="center", ha="center")

# ---- Emitted lines on the right --------------------------------------
def emit_line(ax, v: Visit):
    color = NESTED_COL if v.depth == 2 else TOP_COL
    # Dot
    dot = Circle((X_OUT + 0.002, v.y), 0.008, facecolor=color,
                 edgecolor="none")
    ax.add_patch(dot)
    # Text
    # Build the bind_class<T> string using the qualified name
    qn = v.qualname
    if len(qn) > 24:
        qn_short = qn
    else:
        qn_short = qn
    line = f"bind_class<{qn_short}>"
    ax.text(X_OUT + 0.020, v.y, line,
            fontsize=9.7, color=INK, family=MONO, va="center")

for v in visits:
    emit_line(axT, v)

# "...more..." below emit column
axT.text((X_OUT + X_OUT_END) / 2, more_y,
         "· · · 38 more bind_class<T> lines · · ·",
         fontsize=9.5, color=MUTED, family=MONO, style="italic",
         va="center", ha="center")

# Footer: legend (bottom of panel)
legend_y = 0.02
def chip(ax, x, y, color, text):
    ax.add_patch(Circle((x, y), 0.008, facecolor=color, edgecolor="none"))
    ax.text(x + 0.018, y, text,
            fontsize=9.5, color=DIM, family=MONO,
            va="center", ha="left")

chip(axT, 0.07, legend_y, FILE_COL,   "header file")
chip(axT, 0.22, legend_y, TOP_COL,    "top-level class")
chip(axT, 0.40, legend_y, NESTED_COL, "nested class (Parent::Child)")
# Dashed-line legend entry for the DFS path
axT.plot([0.68, 0.71], [legend_y, legend_y],
         color=PATH_COL, lw=2, linestyle=(0, (4, 3)), alpha=0.9)
axT.text(0.72, legend_y, "DFS traversal order",
         fontsize=9.5, color=DIM, family=MONO,
         va="center", ha="left")

# ---- Save -----------------------------------------------------------------
out_dir = os.path.dirname(os.path.abspath(__file__))
out_path = os.path.join(out_dir, "auto_discovery_traversal.png")
fig.savefig(out_path, dpi=100, facecolor=BG, bbox_inches="tight",
            pad_inches=0.2)
try:
    from PIL import Image
    Image.open(out_path).save(out_path, "PNG", optimize=True)
except ImportError:
    pass

print(f"Rendered {out_path}")
print(f"  Size: {os.path.getsize(out_path) // 1024} KB")
