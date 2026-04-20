"""Generate a polished auto-discovery visualization for the blog.

Shows the three stages of `mirror_bridge generate`:

  1. HEADERS   one rounded card per source file, with a stack of the
               class/struct/enum names it contains.
  2. PARSER    one file zoomed in; colored depth bars in the gutter and
               a discovery badge attached to each `class`/`struct`/`enum`
               line.
  3. BINDINGS  the generated MIRROR_BRIDGE_MODULE block with a colored
               dot per line (green = top level, pink = nested).

Outputs `auto_discovery_traversal.png`.
"""
from __future__ import annotations

import os

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
import matplotlib.patches as mp
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon

# ---- Palette ----------------------------------------------------------
BG        = "#0B1220"
PANEL     = "#151E33"
PANEL_HI  = "#1C2640"
INK       = "#F1F5F9"
DIM       = "#94A3B8"
MUTED     = "#64748B"
GRID      = "#2A3752"

TOP_LEVEL = "#34D399"   # emerald-400
NESTED    = "#F472B6"   # pink-400
FLOW      = "#60A5FA"   # blue-400
KEYWORD   = "#FBBF24"   # amber-400

MONO = "DejaVu Sans Mono"

# ---- Content ----------------------------------------------------------
FILES = [
    ("BoundingVolume.h",       ["OrientedBoundingBox",
                                 "AxisAlignedBoundingBox"]),
    ("PointCloud.h",           ["PointCloud"]),
    ("TriangleMesh.h",         ["TriangleMesh", "Material",
                                 "MaterialParameter"]),
    ("HalfEdgeTriangleMesh.h", ["HalfEdgeTriangleMesh", "HalfEdge"]),
    ("KDTreeSearchParam.h",    ["KDTreeSearchParam", "SearchType"]),
]
MORE_FILES_NOTE = "· · · 33 more files · · ·"

ZOOM_FILE = "HalfEdgeTriangleMesh.h"
# (code, depth_after_this_line, discovery_qualname_or_None)
ZOOM_LINES = [
    ("class HalfEdgeTriangleMesh",         1, "HalfEdgeTriangleMesh"),
    ("      : public MeshBase {",          1, None),
    ("public:",                              1, None),
    ("    struct HalfEdge {",              2, "HalfEdgeTriangleMesh::HalfEdge"),
    ("        int vertex_indices[2];",     2, None),
    ("        int triangle_index;",        2, None),
    ("    };",                               1, None),
    ("    std::vector<HalfEdge> edges_;",  1, None),
    ("};",                                   0, None),
]

BIND_LINES = [
    ("bind_class<OrientedBoundingBox>(m, \"OrientedBoundingBox\");",        False),
    ("bind_class<AxisAlignedBoundingBox>(m, \"AxisAlignedBoundingBox\");",  False),
    ("bind_class<PointCloud>(m, \"PointCloud\");",                          False),
    ("bind_class<HalfEdgeTriangleMesh>(m, \"HalfEdgeTriangleMesh\");",      False),
    ("bind_class<HalfEdgeTriangleMesh::HalfEdge>(m, \"HalfEdge\");",        True),
    ("bind_class<KDTreeSearchParam>(m, \"KDTreeSearchParam\");",            False),
    ("bind_class<KDTreeSearchParam::SearchType>(m, \"SearchType\");",       True),
    ("bind_class<TriangleMesh::Material::MaterialParameter>(m, \"…\");",    True),
    ("// … 39 more bind_class<T> lines …",                                   None),
]

# ---- Figure setup -----------------------------------------------------
fig = plt.figure(figsize=(16, 8.6), facecolor=BG)

# Header strip (title, subtitle, stats) + 3-column body.
outer = fig.add_gridspec(2, 1, height_ratios=[0.14, 0.86],
                         left=0.03, right=0.97, top=0.975, bottom=0.03,
                         hspace=0.015)

# =========================================================
# HEADER BANNER
# =========================================================
axH = fig.add_subplot(outer[0, 0])
axH.set_facecolor(BG)
axH.set_xlim(0, 1); axH.set_ylim(0, 1)
axH.set_xticks([]); axH.set_yticks([])
for s in axH.spines.values():
    s.set_visible(False)

axH.text(0.002, 0.78, "Auto-discovery",
         fontsize=22, color=INK, weight="bold",
         va="center", family=MONO)
axH.text(0.002, 0.32, "C++ headers → Python bindings, zero glue",
         fontsize=12, color=DIM, style="italic",
         va="center", family=MONO)

# Stat pills on the right
def stat(ax, x_center, num, label, color):
    ax.text(x_center, 0.78, num, fontsize=26, color=color,
            weight="bold", va="center", ha="right", family=MONO)
    ax.text(x_center + 0.006, 0.78, label, fontsize=11, color=DIM,
            va="center", ha="left", family=MONO)

stat(axH, 0.49, "47",  "  classes discovered",  TOP_LEVEL)
stat(axH, 0.72, "38",  "  headers scanned",     FLOW)
stat(axH, 0.93, "0",   "  lines of binding code", NESTED)

# Horizontal divider
axH.plot([0.002, 0.998], [0.02, 0.02], color=GRID, lw=0.8)

# =========================================================
# BODY: three columns
# =========================================================
body = outer[1, 0].subgridspec(1, 3, width_ratios=[0.95, 1.2, 1.35],
                               wspace=0.06)

# ---- Panel 1: HEADERS --------------------------------------------------
ax1 = fig.add_subplot(body[0, 0])
ax1.set_facecolor(BG)
ax1.set_xlim(0, 1); ax1.set_ylim(0, 1)
ax1.set_xticks([]); ax1.set_yticks([])
for s in ax1.spines.values():
    s.set_visible(False)

ax1.text(0.02, 0.96, "1  HEADERS", fontsize=11, color=DIM,
         weight="bold", family=MONO, va="top")
ax1.text(0.02, 0.915, "what the tool scans",
         fontsize=9.5, color=MUTED, style="italic", va="top", family=MONO)

def draw_file_card(ax, x, y_top, w, h, filename, types, highlight=False):
    """Card with filename header and a type list inside."""
    body_color = PANEL_HI if highlight else PANEL
    edge_color = FLOW if highlight else GRID
    edge_w     = 1.8 if highlight else 0.9

    card = FancyBboxPatch(
        (x, y_top - h), w, h,
        boxstyle="round,pad=0,rounding_size=0.012",
        linewidth=edge_w, edgecolor=edge_color, facecolor=body_color,
        transform=ax.transAxes,
    )
    ax.add_patch(card)

    # Folded-corner triangle
    fold = 0.028
    tri = Polygon(
        [(x + w - fold, y_top),
         (x + w,        y_top),
         (x + w,        y_top - fold)],
        closed=True, facecolor=edge_color, edgecolor="none",
        transform=ax.transAxes,
    )
    ax.add_patch(tri)

    # Left accent strip
    strip = mp.Rectangle(
        (x, y_top - h), 0.006, h,
        facecolor=FLOW if highlight else MUTED, edgecolor="none",
        transform=ax.transAxes,
    )
    ax.add_patch(strip)

    # Filename header
    ax.text(x + 0.025, y_top - 0.029, filename,
            fontsize=10.5,
            color=INK if highlight else DIM,
            weight="bold", family=MONO,
            transform=ax.transAxes, va="top")

    # Thin separator
    ax.plot([x + 0.025, x + w - 0.025],
            [y_top - 0.052, y_top - 0.052],
            color=GRID, linewidth=0.7, transform=ax.transAxes)

    # Type chips
    yy = y_top - 0.075
    for t in types:
        ax.text(x + 0.035, yy, t, fontsize=9.2,
                color=DIM, family=MONO,
                transform=ax.transAxes, va="top")
        yy -= 0.023

# Stack of cards
card_w = 0.93
card_x = 0.035
card_y = 0.87
base_h = 0.048   # card height minimum
per_type = 0.023
pad_h = 0.085    # header + bottom pad of a card

for fname, types in FILES:
    h = pad_h + per_type * len(types)
    highlight = (fname == ZOOM_FILE)
    draw_file_card(ax1, card_x, card_y, card_w, h, fname, types,
                   highlight=highlight)
    card_y -= (h + 0.020)

# Trailing note
ax1.text(0.5, max(card_y - 0.01, 0.04), MORE_FILES_NOTE,
         fontsize=9.5, color=MUTED, family=MONO, style="italic",
         ha="center", va="top", transform=ax1.transAxes)

# ---- Panel 2: PARSER ---------------------------------------------------
ax2 = fig.add_subplot(body[0, 1])
ax2.set_facecolor(BG)
ax2.set_xlim(0, 1); ax2.set_ylim(0, 1)
ax2.set_xticks([]); ax2.set_yticks([])
for s in ax2.spines.values():
    s.set_visible(False)

ax2.text(0.02, 0.96, "2  BRACE-DEPTH WALK", fontsize=11, color=DIM,
         weight="bold", family=MONO, va="top")
ax2.text(0.02, 0.915, f"inside {ZOOM_FILE}",
         fontsize=9.5, color=MUTED, style="italic",
         va="top", family=MONO)

# Editor-card background
c_x, c_top, c_w, c_h = 0.03, 0.87, 0.94, 0.79
editor = FancyBboxPatch(
    (c_x, c_top - c_h), c_w, c_h,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0.9, edgecolor=GRID, facecolor=PANEL,
    transform=ax2.transAxes,
)
ax2.add_patch(editor)

# Editor title bar
tb = FancyBboxPatch(
    (c_x, c_top - 0.046), c_w, 0.046,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0, facecolor=PANEL_HI, transform=ax2.transAxes,
)
ax2.add_patch(tb)
# Traffic lights
for i, clr in enumerate(["#ef4444", "#f59e0b", "#10b981"]):
    dot = mp.Circle((c_x + 0.018 + i * 0.022, c_top - 0.023),
                    0.007, facecolor=clr, edgecolor="none",
                    transform=ax2.transAxes)
    ax2.add_patch(dot)
ax2.text(c_x + c_w / 2, c_top - 0.023, ZOOM_FILE,
         fontsize=9.5, color=DIM, family=MONO,
         ha="center", va="center", transform=ax2.transAxes)

# Code area
code_top   = c_top - 0.07
code_bot   = c_top - c_h + 0.13
line_h     = (code_top - code_bot) / max(len(ZOOM_LINES), 1)
gutter_x   = c_x + 0.025
code_start = c_x + 0.085
badge_r_x  = c_x + c_w - 0.015   # right edge of the badge

def draw_depth_gutter(ax, x_base, y_center, depth):
    bar_w = 0.012
    bar_h = line_h * 0.60
    for d in range(1, depth + 1):
        col = TOP_LEVEL if d == 1 else NESTED
        bar = mp.Rectangle(
            (x_base + (d - 1) * (bar_w + 0.006), y_center - bar_h / 2),
            bar_w, bar_h,
            facecolor=col, alpha=0.85, edgecolor="none",
            transform=ax.transAxes,
        )
        ax.add_patch(bar)

def draw_badge(ax, right_x, y_center, qualname):
    is_nested = "::" in qualname
    col = NESTED if is_nested else TOP_LEVEL
    label = qualname
    if len(label) > 22:
        label = label[:20] + "…"
    # approx width per character in the badge
    text_w = 0.0085 * len(label)
    pad = 0.022
    badge_w = text_w + 0.035 + pad
    x0 = right_x - badge_w
    box = FancyBboxPatch(
        (x0, y_center - 0.022), badge_w, 0.044,
        boxstyle="round,pad=0,rounding_size=0.010",
        linewidth=0, facecolor=col, alpha=0.18,
        transform=ax.transAxes,
    )
    ax.add_patch(box)
    # edge accent
    accent = mp.Rectangle(
        (x0, y_center - 0.022), 0.004, 0.044,
        facecolor=col, edgecolor="none", transform=ax.transAxes,
    )
    ax.add_patch(accent)
    ax.text(x0 + 0.020, y_center, "✓",
            fontsize=11, color=col, weight="bold",
            transform=ax.transAxes, va="center", ha="center")
    ax.text(x0 + 0.034, y_center, label,
            fontsize=9, color=col, weight="bold", family=MONO,
            transform=ax.transAxes, va="center", ha="left")

for i, (code, depth, discovery) in enumerate(ZOOM_LINES):
    y = code_top - (i + 0.5) * line_h
    # gutter bars
    draw_depth_gutter(ax2, gutter_x, y, depth)
    # one code text (no keyword highlighting, avoids overlap bugs)
    is_open = discovery is not None
    color = INK if not code.lstrip().startswith("//") else DIM
    if is_open:
        # Subtle row highlight
        row = mp.Rectangle(
            (c_x + 0.004, y - line_h * 0.48), c_w - 0.008, line_h * 0.96,
            facecolor=FLOW, alpha=0.06, edgecolor="none",
            transform=ax2.transAxes,
        )
        ax2.add_patch(row)
    ax2.text(code_start, y, code,
             fontsize=10.5, color=color, family=MONO,
             transform=ax2.transAxes, va="center")
    if discovery:
        draw_badge(ax2, badge_r_x, y, discovery)

# Footer: the rule
foot_y = c_top - c_h + 0.06
ax2.plot([c_x + 0.025, c_x + c_w - 0.025],
         [foot_y + 0.042, foot_y + 0.042],
         color=GRID, lw=0.7, transform=ax2.transAxes)
ax2.text(c_x + 0.025, foot_y + 0.018,
         "rule:  on `class`/`struct`/`enum` at a new depth, emit",
         fontsize=9.5, color=INK, family=MONO, weight="bold",
         transform=ax2.transAxes, va="center")
ax2.text(c_x + 0.025, foot_y - 0.017,
         "       prefix[0..depth-1] + ::current_name   →   bind_class<…>",
         fontsize=9.5, color=FLOW, family=MONO, style="italic",
         transform=ax2.transAxes, va="center")

# ---- Panel 3: GENERATED BINDINGS --------------------------------------
ax3 = fig.add_subplot(body[0, 2])
ax3.set_facecolor(BG)
ax3.set_xlim(0, 1); ax3.set_ylim(0, 1)
ax3.set_xticks([]); ax3.set_yticks([])
for s in ax3.spines.values():
    s.set_visible(False)

ax3.text(0.02, 0.96, "3  GENERATED BINDINGS", fontsize=11, color=DIM,
         weight="bold", family=MONO, va="top")
ax3.text(0.02, 0.915, "one bind_class<T> per discovery",
         fontsize=9.5, color=MUTED, style="italic",
         va="top", family=MONO)

# Editor card
o_x, o_top, o_w, o_h = 0.03, 0.87, 0.94, 0.80
out_card = FancyBboxPatch(
    (o_x, o_top - o_h), o_w, o_h,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0.9, edgecolor=GRID, facecolor=PANEL,
    transform=ax3.transAxes,
)
ax3.add_patch(out_card)
# Title bar
tb3 = FancyBboxPatch(
    (o_x, o_top - 0.046), o_w, 0.046,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0, facecolor=PANEL_HI, transform=ax3.transAxes,
)
ax3.add_patch(tb3)
for i, clr in enumerate(["#ef4444", "#f59e0b", "#10b981"]):
    dot = mp.Circle((o_x + 0.018 + i * 0.022, o_top - 0.023),
                    0.007, facecolor=clr, edgecolor="none",
                    transform=ax3.transAxes)
    ax3.add_patch(dot)
ax3.text(o_x + o_w / 2, o_top - 0.023, "open3d_full.cpp",
         fontsize=9.5, color=DIM, family=MONO,
         ha="center", va="center", transform=ax3.transAxes)
ax3.text(o_x + o_w - 0.02, o_top - 0.023, "auto-generated",
         fontsize=8.5, color=TOP_LEVEL, family=MONO,
         weight="bold", ha="right", va="center",
         transform=ax3.transAxes)

# Lines
code_top3 = o_top - 0.07
line_h3   = 0.062
x_code3   = o_x + 0.065
x_dot3    = o_x + 0.032

# module header line
ax3.text(x_code3, code_top3,
         "MIRROR_BRIDGE_MODULE(open3d_full,",
         fontsize=10, color=KEYWORD, family=MONO, weight="bold",
         transform=ax3.transAxes, va="top")

y3 = code_top3 - line_h3 - 0.01
for line, nested in BIND_LINES:
    # coloured dot
    if nested is None:
        dot_col = MUTED   # comment
    elif nested:
        dot_col = NESTED
    else:
        dot_col = TOP_LEVEL
    dot = mp.Circle((x_dot3, y3 - 0.013), 0.008,
                    facecolor=dot_col, edgecolor="none",
                    transform=ax3.transAxes)
    ax3.add_patch(dot)
    color = MUTED if line.startswith("//") else INK
    style = "italic" if line.startswith("//") else "normal"
    ax3.text(x_code3, y3, line,
             fontsize=9.5, color=color, family=MONO, style=style,
             transform=ax3.transAxes, va="top")
    y3 -= line_h3

# Closing paren
ax3.text(x_code3 - 0.03, y3 - 0.01, ")",
         fontsize=10, color=KEYWORD, family=MONO, weight="bold",
         transform=ax3.transAxes, va="top")

# Legend strip at the bottom of the panel
leg_y = o_top - o_h + 0.04
ax3.plot([o_x + 0.025, o_x + o_w - 0.025],
         [leg_y + 0.04, leg_y + 0.04],
         color=GRID, lw=0.7, transform=ax3.transAxes)

def chip(ax, x, y, color, text):
    dot = mp.Circle((x, y), 0.008, facecolor=color, edgecolor="none",
                    transform=ax.transAxes)
    ax.add_patch(dot)
    ax.text(x + 0.018, y, text, fontsize=9.5, color=DIM,
            family=MONO, transform=ax.transAxes, va="center")

chip(ax3, o_x + 0.035, leg_y + 0.0, TOP_LEVEL, "top-level class")
chip(ax3, o_x + 0.35,  leg_y + 0.0, NESTED,    "nested class  (Parent::Child)")

# =========================================================
# FLOW ARROWS
# =========================================================
fig.canvas.draw()
p1 = ax1.get_position()
p2 = ax2.get_position()
p3 = ax3.get_position()

def flow_arrow(fig, x0, x1, y, label):
    arrow = FancyArrowPatch(
        (x0, y), (x1, y),
        arrowstyle="-|>", mutation_scale=26,
        color=FLOW, lw=2.6, alpha=0.95,
        shrinkA=0, shrinkB=0,
        transform=fig.transFigure,
    )
    fig.patches.append(arrow)
    fig.text((x0 + x1) / 2, y + 0.028, label,
             fontsize=10, color=FLOW, family=MONO, weight="bold",
             ha="center", va="bottom", transform=fig.transFigure)

arrow_y = (p1.y0 + p1.y1) / 2
flow_arrow(fig, p1.x1 + 0.004, p2.x0 - 0.004, arrow_y, "parse")
flow_arrow(fig, p2.x1 + 0.004, p3.x0 - 0.004, arrow_y, "emit")

# =========================================================
# SAVE
# =========================================================
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
