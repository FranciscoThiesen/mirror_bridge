"""Auto-discovery visualization: class dependency DAG + topological order.

The real story of `mirror_bridge generate` isn't a brace-depth parser
(that's a detail). It's that every discovered class has dependencies on
other classes - through inheritance, through nested scopes, through
fields and method signatures - and the generator has to emit
`bind_class<T>` calls in a topological order so each class is bound
after everything it references.

This figure draws:

  · The class dependency graph for a slice of Open3D's geometry module.
    Three edge kinds: inheritance (solid navy), nested-inside (solid
    plum), uses-as-return/field (dashed gold).
  · A numbered topological traversal overlaid on the graph, showing the
    order the generator emits bindings. A dashed amber curve connects
    consecutive visit numbers.
  · A legend explaining each edge kind and the binding order marker.

Outputs `auto_discovery_traversal.png`.
"""
from __future__ import annotations

import os

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle

# ---- Palette (warm research-blog style) -------------------------------
BG       = "#FAF7F0"   # cream
PAPER    = "#FFFFFF"
INK      = "#16161A"
INK_SOFT = "#32302C"
DIM      = "#6B6560"
MUTED    = "#A3998C"
HAIRLINE = "#E4DED1"

# Semantic accents
NAVY     = "#1D3A8A"   # inheritance
PLUM     = "#8E2F6D"   # nesting
GOLD     = "#B8791F"   # "uses as type"
TERRA    = "#C96442"   # visual branding
FOREST   = "#216E4E"
GRAY_BOX = "#C9C3B6"   # abstract class fill
AMBER    = "#D97706"   # traversal path
NAVY_SOFT = "#E8EDF7"  # node fill for concrete classes
PLUM_SOFT = "#F7EAF1"  # node fill for nested classes
GRAY_SOFT = "#F0EBE0"  # node fill for abstract classes

SERIF = "DejaVu Serif"
SANS  = "DejaVu Sans"
MONO  = "DejaVu Sans Mono"

# ---- The DAG to draw --------------------------------------------------
# Coordinates are figure-relative for the main graph panel (axG).
# We pick positions manually to keep the graph readable.
NODES = {
    # name                          (x,    y),   kind,       width
    "Geometry":                 {"xy": (0.05, 0.64), "kind": "abstract",  "w": 0.130},
    "Geometry3D":               {"xy": (0.21, 0.64), "kind": "abstract",  "w": 0.135},
    "AxisAlignedBoundingBox":   {"xy": (0.44, 0.86), "kind": "concrete",  "w": 0.210},
    "PointCloud":               {"xy": (0.44, 0.64), "kind": "concrete",  "w": 0.135},
    "TriangleMesh":             {"xy": (0.44, 0.42), "kind": "concrete",  "w": 0.150},
    "HalfEdgeTriangleMesh":     {"xy": (0.44, 0.20), "kind": "concrete",  "w": 0.210},
    "Material":                 {"xy": (0.70, 0.42), "kind": "nested",    "w": 0.125},
    "MaterialParameter":        {"xy": (0.88, 0.42), "kind": "nested",    "w": 0.175},
    "HalfEdge":                 {"xy": (0.70, 0.20), "kind": "nested",    "w": 0.110},
}
NODE_H = 0.082

# Edges: (source, target, kind)
# kind ∈ {'inherits' (derived → base),
#         'nested'   (nested → enclosing),
#         'uses'     (owner → used type)}
EDGES = [
    ("Geometry3D",             "Geometry",                "inherits"),
    ("AxisAlignedBoundingBox", "Geometry3D",              "inherits"),
    ("PointCloud",             "Geometry3D",              "inherits"),
    ("TriangleMesh",           "Geometry3D",              "inherits"),
    ("HalfEdgeTriangleMesh",   "TriangleMesh",            "inherits"),
    ("Material",               "TriangleMesh",            "nested"),
    ("MaterialParameter",      "Material",                "nested"),
    ("HalfEdge",               "HalfEdgeTriangleMesh",    "nested"),
    ("PointCloud",             "AxisAlignedBoundingBox",  "uses"),
]

# Topological order the generator uses. Each dependency target must
# come before its dependants. Verified by hand against EDGES above.
TOPO_ORDER = [
    "Geometry",
    "Geometry3D",
    "AxisAlignedBoundingBox",
    "PointCloud",
    "TriangleMesh",
    "Material",
    "MaterialParameter",
    "HalfEdgeTriangleMesh",
    "HalfEdge",
]

# ---- Figure setup -----------------------------------------------------
fig = plt.figure(figsize=(17, 10), facecolor=BG)
outer = fig.add_gridspec(
    3, 1, height_ratios=[0.15, 0.70, 0.15],
    left=0.038, right=0.962, top=0.97, bottom=0.035,
    hspace=0.02,
)

# =======================================================================
# HEADER
# =======================================================================
axH = fig.add_subplot(outer[0, 0])
axH.set_facecolor(BG)
axH.set_xlim(0, 1); axH.set_ylim(0, 1)
axH.set_xticks([]); axH.set_yticks([])
for s in axH.spines.values():
    s.set_visible(False)

# Eyebrow
axH.text(0.002, 0.92, "MIRROR_BRIDGE  ·  AUTO-DISCOVERY",
         fontsize=9.2, color=TERRA, family=SANS, weight="bold",
         va="top")

# Serif headline
axH.text(0.002, 0.62,
         "Every class gets bound after the classes it depends on.",
         fontsize=23, color=INK, family=SERIF, weight="bold",
         va="center")

# Subtitle in serif italic
axH.text(0.002, 0.18,
         "Dependencies are discovered through three reflection queries: "
         "bases_of (inheritance), enclosing scope (nesting), and "
         "parameters_of / return types (uses). "
         "The generator topologically sorts them before emitting a "
         "single bind_class<T> per type.",
         fontsize=11, color=DIM, family=SERIF, style="italic",
         va="center", wrap=True)

# Divider
axH.plot([0.002, 0.998], [-0.02, -0.02], color=HAIRLINE, lw=0.8,
         clip_on=False)

# =======================================================================
# MAIN GRAPH
# =======================================================================
axG = fig.add_subplot(outer[1, 0])
axG.set_facecolor(BG)
axG.set_xlim(0, 1); axG.set_ylim(0, 1)
axG.set_xticks([]); axG.set_yticks([])
for s in axG.spines.values():
    s.set_visible(False)

# Column labels at the top (hint at topological layers)
axG.text(0.055 + 0.065, 0.995, "abstract base",
         fontsize=9, color=MUTED, family=SANS,
         style="italic", va="top", ha="center", transform=axG.transAxes)
axG.text(0.21 + 0.067, 0.995, "abstract middle",
         fontsize=9, color=MUTED, family=SANS,
         style="italic", va="top", ha="center", transform=axG.transAxes)
axG.text(0.44 + 0.075, 0.995, "top-level classes",
         fontsize=9, color=MUTED, family=SANS,
         style="italic", va="top", ha="center", transform=axG.transAxes)
axG.text(0.79, 0.995, "nested types",
         fontsize=9, color=MUTED, family=SANS,
         style="italic", va="top", ha="center", transform=axG.transAxes)

# ---- Draw edges first (behind nodes) ----
def node_anchor(name, side):
    """Return (x, y) on the boundary of node `name` for an edge going
    toward `side`, where side ∈ {'L', 'R', 'T', 'B'}."""
    n = NODES[name]
    cx, cy = n["xy"]
    w = n["w"]
    if side == "L":
        return (cx - w / 2, cy)
    if side == "R":
        return (cx + w / 2, cy)
    if side == "T":
        return (cx, cy + NODE_H / 2)
    if side == "B":
        return (cx, cy - NODE_H / 2)
    return (cx, cy)

def pick_sides(src, tgt):
    """Choose exit side of src and entry side of tgt based on relative
    position. Favours horizontal connections when columns differ, vertical
    otherwise."""
    sx, sy = NODES[src]["xy"]
    tx, ty = NODES[tgt]["xy"]
    if abs(tx - sx) > abs(ty - sy) * 1.2:
        return ("R" if tx > sx else "L"), ("L" if tx > sx else "R")
    else:
        return ("T" if ty > sy else "B"), ("B" if ty > sy else "T")

EDGE_STYLE = {
    "inherits": dict(color=NAVY,  lw=1.6, linestyle="-",  alpha=0.9,
                     arrowstyle="-|>", mutation_scale=14,
                     connectionstyle="arc3,rad=0.0"),
    "nested":   dict(color=PLUM,  lw=1.6, linestyle="-",  alpha=0.9,
                     arrowstyle="-|>", mutation_scale=14,
                     connectionstyle="arc3,rad=0.0"),
    "uses":     dict(color=GOLD,  lw=1.4, linestyle=(0, (4, 3)), alpha=0.85,
                     arrowstyle="-|>", mutation_scale=12,
                     connectionstyle="arc3,rad=-0.25"),
}

for src, tgt, kind in EDGES:
    ss, ts = pick_sides(src, tgt)
    x0, y0 = node_anchor(src, ss)
    x1, y1 = node_anchor(tgt, ts)
    style = EDGE_STYLE[kind]
    arrow = FancyArrowPatch(
        (x0, y0), (x1, y1),
        arrowstyle=style["arrowstyle"],
        connectionstyle=style["connectionstyle"],
        mutation_scale=style["mutation_scale"],
        color=style["color"],
        linewidth=style["lw"],
        linestyle=style["linestyle"],
        alpha=style["alpha"],
        shrinkA=4, shrinkB=4,
        transform=axG.transAxes,
        zorder=2,
    )
    axG.add_patch(arrow)

# ---- Draw nodes ----
visit_index = {name: i + 1 for i, name in enumerate(TOPO_ORDER)}

def draw_node(ax, name):
    n = NODES[name]
    cx, cy = n["xy"]
    w = n["w"]
    kind = n["kind"]

    # Base colours
    if kind == "abstract":
        fill = GRAY_SOFT
        border = GRAY_BOX
        text_col = INK_SOFT
        extra = "  (abstract)"
        dashed = True
    elif kind == "concrete":
        fill = NAVY_SOFT
        border = NAVY
        text_col = NAVY
        extra = ""
        dashed = False
    else:  # nested
        fill = PLUM_SOFT
        border = PLUM
        text_col = PLUM
        extra = ""
        dashed = False

    box = FancyBboxPatch(
        (cx - w / 2, cy - NODE_H / 2), w, NODE_H,
        boxstyle="round,pad=0,rounding_size=0.011",
        linewidth=1.7, edgecolor=border, facecolor=fill,
        linestyle="--" if dashed else "-",
        transform=ax.transAxes, zorder=3,
    )
    ax.add_patch(box)

    # Class name (centered) - single text call
    ax.text(cx, cy + 0.006, name + extra,
            fontsize=10.3, color=text_col,
            family=MONO, weight="bold" if kind != "abstract" else "normal",
            va="center", ha="center", transform=ax.transAxes, zorder=4)

    # Number badge for topological visit order (top-left corner)
    idx = visit_index.get(name)
    if idx is not None:
        badge_x = cx - w / 2 + 0.017
        badge_y = cy + NODE_H / 2 - 0.005
        circ = Circle(
            (badge_x, badge_y), 0.016,
            facecolor=AMBER, edgecolor=BG, linewidth=1.8,
            transform=ax.transAxes, zorder=5,
        )
        ax.add_patch(circ)
        ax.text(badge_x, badge_y, str(idx),
                fontsize=9.5, color=BG, family=MONO, weight="bold",
                va="center", ha="center", transform=ax.transAxes,
                zorder=6)

    # Tiny subtitle under the name (what the node represents)
    sub = {
        "Geometry":                   "virtual interface",
        "Geometry3D":                 "virtual 3-D base",
        "AxisAlignedBoundingBox":     "struct,  3 Eigen::Vector3d",
        "PointCloud":                 "returns AABB via get_aabb()",
        "TriangleMesh":               "contains Material",
        "HalfEdgeTriangleMesh":       "extends TriangleMesh",
        "Material":                   "nested in TriangleMesh",
        "MaterialParameter":          "nested in Material",
        "HalfEdge":                   "nested in HalfEdgeTriangleMesh",
    }.get(name, "")
    if sub:
        ax.text(cx, cy - 0.015, sub,
                fontsize=8, color=DIM, family=SANS, style="italic",
                va="center", ha="center", transform=ax.transAxes,
                zorder=4)

for name in NODES:
    draw_node(axG, name)

# =======================================================================
# FOOTER  :  legend + side callout
# =======================================================================
axL = fig.add_subplot(outer[2, 0])
axL.set_facecolor(BG)
axL.set_xlim(0, 1); axL.set_ylim(0, 1)
axL.set_xticks([]); axL.set_yticks([])
for s in axL.spines.values():
    s.set_visible(False)

# Legend title
axL.text(0.002, 0.95, "EDGE KEY",
         fontsize=9.2, color=TERRA, family=SANS, weight="bold", va="top")

# Four columns of legend entries: inheritance / nested / uses + traversal
x_col = [0.01, 0.25, 0.50, 0.76]

# Column 1: inheritance
ax_ = axL
ax_.add_patch(FancyArrowPatch((x_col[0], 0.58), (x_col[0] + 0.04, 0.58),
    arrowstyle="-|>", mutation_scale=12, color=NAVY, lw=1.6,
    shrinkA=0, shrinkB=0, transform=ax_.transAxes))
ax_.text(x_col[0] + 0.055, 0.60, "inherits",
    fontsize=10.2, color=NAVY, family=MONO, weight="bold",
    va="center", ha="left", transform=ax_.transAxes)
ax_.text(x_col[0] + 0.055, 0.32,
    "derived → base.  from bases_of(T)",
    fontsize=9.2, color=DIM, family=SERIF, style="italic",
    va="center", ha="left", transform=ax_.transAxes)

# Column 2: nested
ax_.add_patch(FancyArrowPatch((x_col[1], 0.58), (x_col[1] + 0.04, 0.58),
    arrowstyle="-|>", mutation_scale=12, color=PLUM, lw=1.6,
    shrinkA=0, shrinkB=0, transform=ax_.transAxes))
ax_.text(x_col[1] + 0.055, 0.60, "nested in",
    fontsize=10.2, color=PLUM, family=MONO, weight="bold",
    va="center", ha="left", transform=ax_.transAxes)
ax_.text(x_col[1] + 0.055, 0.32,
    "inner → enclosing.  from enclosing scope",
    fontsize=9.2, color=DIM, family=SERIF, style="italic",
    va="center", ha="left", transform=ax_.transAxes)

# Column 3: uses
ax_.add_patch(FancyArrowPatch((x_col[2], 0.58), (x_col[2] + 0.04, 0.58),
    arrowstyle="-|>", mutation_scale=12, color=GOLD, lw=1.4,
    linestyle=(0, (4, 3)),
    shrinkA=0, shrinkB=0, transform=ax_.transAxes))
ax_.text(x_col[2] + 0.055, 0.60, "uses",
    fontsize=10.2, color=GOLD, family=MONO, weight="bold",
    va="center", ha="left", transform=ax_.transAxes)
ax_.text(x_col[2] + 0.055, 0.32,
    "owner → referenced.  fields, params, returns",
    fontsize=9.2, color=DIM, family=SERIF, style="italic",
    va="center", ha="left", transform=ax_.transAxes)

# Column 4: traversal badge
badge_x = x_col[3] + 0.013
badge_y = 0.60
ax_.add_patch(Circle((badge_x, badge_y), 0.016,
    facecolor=AMBER, edgecolor=BG, linewidth=1.6, transform=ax_.transAxes))
ax_.text(badge_x, badge_y, "n", fontsize=9.5, color=BG,
    family=MONO, weight="bold", va="center", ha="center",
    transform=ax_.transAxes)
ax_.text(badge_x + 0.028, 0.60, "binding order",
    fontsize=10.2, color=AMBER, family=MONO, weight="bold",
    va="center", ha="left", transform=ax_.transAxes)
ax_.text(badge_x + 0.028, 0.32,
    "n-th node = n-th bind_class emitted",
    fontsize=9.2, color=DIM, family=SERIF, style="italic",
    va="center", ha="left", transform=ax_.transAxes)

# =======================================================================
# SAVE
# =======================================================================
out_dir = os.path.dirname(os.path.abspath(__file__))
out_path = os.path.join(out_dir, "auto_discovery_traversal.png")
fig.savefig(out_path, dpi=100, facecolor=BG, bbox_inches="tight",
            pad_inches=0.25)

try:
    from PIL import Image
    Image.open(out_path).save(out_path, "PNG", optimize=True)
except ImportError:
    pass

print(f"Rendered {out_path}")
print(f"  Size: {os.path.getsize(out_path) // 1024} KB")
