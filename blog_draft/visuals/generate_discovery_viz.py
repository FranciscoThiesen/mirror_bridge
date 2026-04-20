"""Auto-discovery visualization in the style of Anthropic / DeepMind
research blog posts: warm cream background, sparing accent colours, real
syntax-highlighted C++ on the left, a receipt-style emission tape on the
right, and a depth-stack timeline beneath that reveals the algorithm.

Outputs `auto_discovery_traversal.png`.
"""
from __future__ import annotations

import os

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle, Circle

# ---- Palette (warm, magazine-quality) ---------------------------------
BG       = "#FAF7F0"   # cream
PAPER    = "#FFFFFF"   # panel interior (code windows, cards)
INK      = "#16161A"   # near-black charcoal
INK_SOFT = "#32302C"
DIM      = "#6B6560"   # warm gray
MUTED    = "#A3998C"
HAIRLINE = "#E4DED1"

# Accents, used sparingly
TERRA    = "#C96442"   # terracotta (Anthropic-ish primary)
NAVY     = "#1D3A8A"   # files
FOREST   = "#216E4E"   # top-level class
PLUM     = "#8E2F6D"   # nested class
GOLD     = "#B8791F"   # algorithm highlights

# Code syntax colours (muted, paper-friendly)
KW       = "#8E2F6D"   # plum for keywords
TYPE     = "#1D3A8A"   # navy for type names
STR      = "#216E4E"   # green for strings
CMT      = "#9E968B"   # gray for comments

SERIF = "DejaVu Serif"
SANS  = "DejaVu Sans"
MONO  = "DejaVu Sans Mono"

# ---- Content ----------------------------------------------------------
# A real, compact source slice that demonstrates both a depth-1 class and
# a depth-2 nested struct. Each entry: (code, depth_after, discovery_qn).
ZOOM_FILE = "HalfEdgeTriangleMesh.h"
LINES = [
    ("// Compact view. Full file: github.com/isl-org/Open3D",  0, None),
    ("",                                                        0, None),
    ("class HalfEdgeTriangleMesh",                              1, "HalfEdgeTriangleMesh"),
    ("      : public MeshBase {",                               1, None),
    ("public:",                                                  1, None),
    ("    struct HalfEdge {",                                   2, "HalfEdgeTriangleMesh::HalfEdge"),
    ("        int vertex_indices[2];",                          2, None),
    ("        int triangle_index;",                             2, None),
    ("    };",                                                   1, None),
    ("    std::vector<HalfEdge> edges_;",                       1, None),
    ("};",                                                       0, None),
    ("",                                                        0, None),
    ("// next file →  KDTreeSearchParam.h ...",                 0, None),
]

# Emissions in DFS order across the whole geometry module. The first
# eight are annotated concretely; everything afterwards is elided.
EMISSIONS = [
    ("01", "bind_class<OrientedBoundingBox>",                    "top"),
    ("02", "bind_class<AxisAlignedBoundingBox>",                 "top"),
    ("03", "bind_class<PointCloud>",                             "top"),
    ("04", "bind_class<TriangleMesh>",                           "top"),
    ("05", "bind_class<Material>",                               "top"),
    ("06", "bind_class<Material::MaterialParameter>",            "nested"),
    ("07", "bind_class<HalfEdgeTriangleMesh>",                   "top"),
    ("08", "bind_class<HalfEdgeTriangleMesh::HalfEdge>",         "nested"),
    ("…",  "… 39 more bind_class<T> calls",                      "more"),
]

# Depth-over-time series for the bottom timeline. Each tick = one token.
# Rises as the parser enters a scope, falls when it leaves. Annotations
# mark discovery events with their emission index.
DEPTH_SERIES = [
    (0, 0, None),                                   # start
    (1, 0, None),                                   # before BV.h
    (2, 1, "01"),                                   # enter OrientedBoundingBox
    (3, 1, None),
    (4, 0, None),                                   # exit
    (5, 1, "02"),                                   # enter AABB
    (6, 0, None),
    (7, 0, None),                                   # gap
    (8, 1, "03"),                                   # PointCloud
    (9, 0, None),
    (10, 1, "04"),                                  # TriangleMesh
    (11, 0, None),
    (12, 1, "05"),                                  # Material
    (13, 2, "06"),                                  # Material::MatParam (nested)
    (14, 1, None),
    (15, 0, None),
    (16, 1, "07"),                                  # HalfEdgeTriangleMesh
    (17, 2, "08"),                                  # HalfEdge (nested)
    (18, 1, None),
    (19, 0, None),
    (20, 0, None),                                  # ... more files
    (21, 1, None),
    (22, 1, None),
    (23, 0, None),
]

# ---- Figure -----------------------------------------------------------
fig = plt.figure(figsize=(16, 10.4), facecolor=BG)
# Overall structure: header (h), main body (m), timeline footer (t)
outer = fig.add_gridspec(3, 1, height_ratios=[0.12, 0.60, 0.28],
                         left=0.038, right=0.962, top=0.97, bottom=0.035,
                         hspace=0.06)

# =======================================================================
# HEADER
# =======================================================================
axH = fig.add_subplot(outer[0, 0])
axH.set_facecolor(BG)
axH.set_xlim(0, 1); axH.set_ylim(0, 1)
axH.set_xticks([]); axH.set_yticks([])
for s in axH.spines.values():
    s.set_visible(False)

# Small eyebrow label
axH.text(0.002, 0.88, "MIRROR_BRIDGE  ·  AUTO-DISCOVERY",
         fontsize=9.2, color=TERRA, family=SANS, weight="bold",
         va="top", ha="left")

# Big serif title + subtitle
axH.text(0.002, 0.58, "A single depth-first pass through the source tree.",
         fontsize=24, color=INK, family=SERIF, weight="bold",
         va="center", ha="left")
axH.text(0.002, 0.10,
         "Open3D's geometry headers are consumed once; every "
         "class, struct and enum becomes a bind_class<T> line.",
         fontsize=12.5, color=DIM, family=SERIF, style="italic",
         va="center", ha="left")

# Inline stats: three big numbers, each paired with its caption directly
# beneath, with generous horizontal spacing so labels don't crowd each
# other.
def stat(ax, x_anchor, big, small, color):
    # Anchor numbers against their own right edge, captions against their
    # own left edge starting at the same anchor. Gives consistent spacing
    # regardless of how many digits the number has.
    ax.text(x_anchor, 0.66, big, fontsize=28, color=color, family=SERIF,
            weight="bold", va="center", ha="left")
    ax.text(x_anchor, 0.18, small, fontsize=9.2, color=DIM,
            family=SANS, va="center", ha="left")

# Spread three stats across the right 30% of the header, with room for the
# longest caption ("lines of hand-written binding") on the far right.
stat(axH, 0.70, "47",  "classes discovered",              FOREST)
stat(axH, 0.79, "38",  "headers scanned",                 NAVY)
stat(axH, 0.88, "0",   "hand-written lines",              TERRA)

# Divider
axH.plot([0.002, 0.998], [-0.06, -0.06], color=HAIRLINE, lw=0.8,
         clip_on=False)

# =======================================================================
# MAIN BODY  :  code window (left 58%) + emission tape (right 42%)
# =======================================================================
body = outer[1, 0].subgridspec(1, 2, width_ratios=[1.35, 1.0], wspace=0.05)

# -----------------------------------------------------------------------
# Left: syntax-highlighted code window with depth gutter and discovery markers
# -----------------------------------------------------------------------
axC = fig.add_subplot(body[0, 0])
axC.set_facecolor(BG)
axC.set_xlim(0, 1); axC.set_ylim(0, 1)
axC.set_xticks([]); axC.set_yticks([])
for s in axC.spines.values():
    s.set_visible(False)

# Panel eyebrow + heading
axC.text(0.0, 0.98, "1  THE PARSER'S VIEW",
         fontsize=10, color=TERRA, weight="bold", family=SANS, va="top")
axC.text(0.0, 0.945, f"zoomed in on {ZOOM_FILE}",
         fontsize=10, color=DIM, family=SANS, style="italic", va="top")

# Card
cx, cy_top, cw, ch = 0.0, 0.895, 1.0, 0.86
card = FancyBboxPatch(
    (cx, cy_top - ch), cw, ch,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0.9, edgecolor=HAIRLINE, facecolor=PAPER,
    transform=axC.transAxes,
)
axC.add_patch(card)
# Title bar (lighter cream)
tb_h = 0.06
tb = FancyBboxPatch(
    (cx, cy_top - tb_h), cw, tb_h,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0, facecolor="#F1EBDA", transform=axC.transAxes,
)
axC.add_patch(tb)
axC.text(cx + cw / 2, cy_top - tb_h / 2, ZOOM_FILE,
         fontsize=11, color=INK_SOFT, family=MONO,
         va="center", ha="center", transform=axC.transAxes)
# Breadcrumb-style path on left edge of title bar
axC.text(cx + 0.02, cy_top - tb_h / 2,
         "open3d / geometry /",
         fontsize=9, color=DIM, family=SANS, style="italic",
         va="center", ha="left", transform=axC.transAxes)

def draw_code_line(ax, line_num, code, depth, discovery, y):
    """Render one code line: gutter line number, stacked depth bars, the
    code itself (single text call, uniform colour for reliability), and
    an optional discovery pill on the right edge.

    Comments render in a muted grey; everything else renders in the ink
    tone. Semantic colour is carried by the depth bars in the gutter and
    by the discovery pill.
    """
    # Line number in the far-left gutter
    ax.text(0.022, y, f"{line_num:>2}",
            fontsize=9, color=MUTED, family=MONO,
            va="center", ha="right", transform=ax.transAxes)

    # Depth bars: one bar per active scope level, coloured by depth
    bar_w = 0.010
    bar_x0 = 0.032
    for d in range(1, depth + 1):
        col = FOREST if d == 1 else PLUM
        rect = Rectangle(
            (bar_x0 + (d - 1) * (bar_w + 0.004), y - 0.014),
            bar_w, 0.028,
            facecolor=col, alpha=0.85 if discovery else 0.55,
            edgecolor="none", transform=ax.transAxes,
        )
        ax.add_patch(rect)

    # Code text: single uniform-colour text call. Comments dim; code ink.
    is_comment = code.lstrip().startswith("//")
    text_color = CMT if is_comment else INK
    font_style = "italic" if is_comment else "normal"
    ax.text(0.082, y, code,
            fontsize=10.5, color=text_color,
            family=MONO, style=font_style,
            va="center", ha="left", transform=ax.transAxes)

    # Discovery pill on the right edge
    if discovery:
        is_nested = "::" in discovery
        col = PLUM if is_nested else FOREST
        label = discovery if len(discovery) <= 28 else discovery[:26] + "…"
        # Approximate pill width from label length so it never overflows
        text_w = 0.0065 * len(label) + 0.045
        pill_x = 1.0 - 0.02 - text_w
        pill = FancyBboxPatch(
            (pill_x, y - 0.017), text_w, 0.034,
            boxstyle="round,pad=0,rounding_size=0.008",
            linewidth=0, facecolor=col, alpha=0.12,
            transform=ax.transAxes,
        )
        ax.add_patch(pill)
        # Left accent bar
        ax.add_patch(Rectangle(
            (pill_x, y - 0.017), 0.003, 0.034,
            facecolor=col, edgecolor="none", transform=ax.transAxes,
        ))
        ax.text(pill_x + 0.017, y, "✓ " + label,
                fontsize=8.8, color=col, family=MONO, weight="bold",
                va="center", ha="left", transform=ax.transAxes)

# Compute line positions
n_lines = len(LINES)
code_top = cy_top - tb_h - 0.03
code_bot = cy_top - ch + 0.04
line_spacing = (code_top - code_bot) / max(n_lines, 1)

for i, (code, depth, discovery) in enumerate(LINES):
    y = code_top - (i + 0.5) * line_spacing
    draw_code_line(axC, i + 1, code, depth, discovery, y)

# -----------------------------------------------------------------------
# Right: "Emission tape" showing bind_class<T> lines in visit order
# -----------------------------------------------------------------------
axE = fig.add_subplot(body[0, 1])
axE.set_facecolor(BG)
axE.set_xlim(0, 1); axE.set_ylim(0, 1)
axE.set_xticks([]); axE.set_yticks([])
for s in axE.spines.values():
    s.set_visible(False)

axE.text(0.0, 0.98, "2  EMISSIONS  (in visit order)",
         fontsize=10, color=TERRA, weight="bold", family=SANS, va="top")
axE.text(0.0, 0.945, "one bind_class<T> per discovery",
         fontsize=10, color=DIM, family=SANS, style="italic", va="top")

# Card background (slightly warmer to contrast with code pane)
ex, ey_top, ew, eh = 0.0, 0.895, 1.0, 0.86
ecard = FancyBboxPatch(
    (ex, ey_top - eh), ew, eh,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0.9, edgecolor=HAIRLINE, facecolor=PAPER,
    transform=axE.transAxes,
)
axE.add_patch(ecard)

# Title strip
tb2 = FancyBboxPatch(
    (ex, ey_top - tb_h), ew, tb_h,
    boxstyle="round,pad=0,rounding_size=0.012",
    linewidth=0, facecolor="#F1EBDA", transform=axE.transAxes,
)
axE.add_patch(tb2)
axE.text(ex + 0.02, ey_top - tb_h / 2, "open3d_full.cpp",
         fontsize=11, color=INK_SOFT, family=MONO, weight="bold",
         va="center", ha="left", transform=axE.transAxes)
axE.text(ex + ew - 0.02, ey_top - tb_h / 2, "AUTO-GENERATED",
         fontsize=8.6, color=TERRA, family=SANS, weight="bold",
         va="center", ha="right", transform=axE.transAxes)

# Emission rows
em_top = ey_top - tb_h - 0.03
em_bot = ey_top - eh + 0.08
em_rows = len(EMISSIONS)
em_sp = (em_top - em_bot) / max(em_rows, 1)

for i, (num, text, kind) in enumerate(EMISSIONS):
    y = em_top - (i + 0.5) * em_sp

    # Index pill on the far left
    if kind != "more":
        pill_w = 0.055
        pill = FancyBboxPatch(
            (0.025, y - 0.019), pill_w, 0.038,
            boxstyle="round,pad=0,rounding_size=0.008",
            linewidth=0, facecolor=GOLD, alpha=0.10,
            transform=axE.transAxes,
        )
        axE.add_patch(pill)
        axE.text(0.025 + pill_w / 2, y, num,
                 fontsize=9.5, color=GOLD, family=MONO, weight="bold",
                 va="center", ha="center", transform=axE.transAxes)
    else:
        axE.text(0.052, y, num, fontsize=11, color=MUTED,
                 family=MONO, va="center", ha="center",
                 transform=axE.transAxes)

    # Kind dot
    if kind in {"top", "nested"}:
        dot_col = FOREST if kind == "top" else PLUM
        dot = Circle((0.112, y), 0.010, facecolor=dot_col,
                     edgecolor="none", transform=axE.transAxes)
        axE.add_patch(dot)

    # Emission text: always one uniform text call so there's no chance of
    # manual positioning drifting out of sync with matplotlib's actual
    # glyph advance.
    if kind == "more":
        axE.text(0.135, y, text,
                 fontsize=10.5, color=DIM, family=MONO, style="italic",
                 va="center", ha="left", transform=axE.transAxes)
    else:
        axE.text(0.135, y, text,
                 fontsize=10.5, color=INK, family=MONO,
                 va="center", ha="left", transform=axE.transAxes)

# Legend inside the emission card (bottom strip)
leg_y = ey_top - eh + 0.04
axE.plot([ex + 0.025, ex + ew - 0.025],
         [leg_y + 0.030, leg_y + 0.030],
         color=HAIRLINE, lw=0.7, transform=axE.transAxes)
Circle_ = Circle  # alias
axE.add_patch(Circle_((0.040, leg_y), 0.009, facecolor=FOREST,
                      edgecolor="none", transform=axE.transAxes))
axE.text(0.063, leg_y, "top-level", fontsize=9.5, color=DIM,
         family=SANS, va="center", ha="left", transform=axE.transAxes)
axE.add_patch(Circle_((0.215, leg_y), 0.009, facecolor=PLUM,
                      edgecolor="none", transform=axE.transAxes))
axE.text(0.238, leg_y, "nested (Parent::Child)", fontsize=9.5,
         color=DIM, family=SANS, va="center", ha="left",
         transform=axE.transAxes)

# =======================================================================
# FOOTER  :  depth-stack timeline (reveals the mechanism)
# =======================================================================
axT = fig.add_subplot(outer[2, 0])
axT.set_facecolor(BG)
for s in axT.spines.values():
    s.set_visible(False)

axT.text(0, 1.17, "3  DEPTH-STACK TIMELINE",
         fontsize=10, color=TERRA, weight="bold", family=SANS,
         transform=axT.transAxes, va="bottom", clip_on=False)
axT.text(0, 1.03,
         "The brace-depth parser maintains a scope stack. "
         "Every new `class` / `struct` / `enum` at a new depth triggers an emission, "
         "qualified by prefix stack.",
         fontsize=10.5, color=DIM, family=SERIF, style="italic",
         transform=axT.transAxes, va="bottom", clip_on=False)

# Data for the chart
xs = [p[0] for p in DEPTH_SERIES]
ys = [p[1] for p in DEPTH_SERIES]

axT.set_xlim(min(xs) - 0.3, max(xs) + 0.3)
axT.set_ylim(-0.12, 2.55)
axT.set_yticks([0, 1, 2])
axT.set_yticklabels(["0", "1", "2"], color=DIM, fontsize=9, family=MONO)
axT.tick_params(axis="y", colors=DIM, length=0)
axT.set_xticks([])
axT.grid(axis="y", color=HAIRLINE, linestyle="-", linewidth=0.6, alpha=0.8)
axT.set_axisbelow(True)

# Curve + filled area (soft terracotta)
axT.plot(xs, ys, drawstyle="steps-post", color=TERRA, linewidth=2.2,
         solid_capstyle="round")
axT.fill_between(xs, 0, ys, step="post", color=TERRA, alpha=0.10)

# Discovery markers with emission index
annotated_top_y_flip = False
for x, y, tag in DEPTH_SERIES:
    if tag is None:
        continue
    col = PLUM if y >= 2 else FOREST
    axT.plot(x, y, marker="o", markersize=10, markerfacecolor=col,
             markeredgecolor=BG, markeredgewidth=1.6, zorder=5)
    # Label placement: alternate above/below the point for readability
    offset_y = 0.28
    axT.annotate(
        f"#{tag}",
        xy=(x, y), xytext=(x, y + offset_y),
        ha="center", va="center", fontsize=8.8, color=col,
        family=MONO, weight="bold",
    )

# Stage annotations beneath the curve
stage_color = GOLD
def stage(x, text):
    axT.axvline(x, ymin=0.02, ymax=0.15, color=stage_color, linewidth=1.0,
                alpha=0.7, linestyle=":")
    axT.text(x, -0.05, text, ha="center", va="top",
             fontsize=8.6, color=stage_color, family=MONO, style="italic")

stage(4.0,  "BoundingVolume.h")
stage(9.0,  "PointCloud.h")
stage(13.0, "TriangleMesh.h")
stage(17.0, "HalfEdgeTriangleMesh.h")
stage(22.0, "· · · 34 more files · · ·")

# Axis labels
axT.text(-0.012, 1.0, "depth", rotation=90, fontsize=9, color=DIM,
         family=SANS, va="top", ha="right", transform=axT.transAxes)
axT.text(1.00, -0.28, "token stream →",
         fontsize=9, color=DIM, family=SANS, ha="right",
         transform=axT.transAxes)

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
