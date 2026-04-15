"""Test Open3D-style geometry classes bound via mirror_bridge.

This exercises the same workflow as real Open3D Python code:
create point clouds, compute bounding boxes, manipulate meshes.
"""

import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import open3d_geometry as o3d

def test_point_cloud():
    pc = o3d.PointCloud()
    assert pc.is_empty()
    assert pc.size() == 0

    # Add points
    pc.points = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]]
    assert pc.has_points()
    assert pc.size() == 4

    center = pc.get_center()
    assert abs(center[0] - 0.25) < 1e-10
    assert abs(center[1] - 0.25) < 1e-10
    assert abs(center[2] - 0.25) < 1e-10

    # Bounding box
    bbox = pc.get_axis_aligned_bounding_box()
    assert abs(bbox.volume() - 1.0) < 1e-10

    # Paint and translate
    pc.paint_uniform_color([0.5, 0.5, 0.5])
    assert pc.has_colors()

    pc.translate([10, 0, 0])
    new_center = pc.get_center()
    assert abs(new_center[0] - 10.25) < 1e-10

    print("  PASS: PointCloud")

def test_bounding_box():
    bbox = o3d.AxisAlignedBoundingBox([0, 0, 0], [1, 2, 3])
    center = bbox.get_center()
    assert abs(center[0] - 0.5) < 1e-10
    assert abs(center[1] - 1.0) < 1e-10
    assert abs(center[2] - 1.5) < 1e-10
    assert abs(bbox.volume() - 6.0) < 1e-10
    print("  PASS: AxisAlignedBoundingBox")

def test_line_set():
    ls = o3d.LineSet()
    ls.points = [[0, 0, 0], [1, 0, 0], [0, 1, 0]]
    ls.lines = [[0, 1], [1, 2], [2, 0]]
    assert ls.has_points()
    assert ls.has_lines()
    ls.paint_uniform_color([1, 0, 0])
    assert ls.has_colors()
    print("  PASS: LineSet")

def test_triangle_mesh():
    mesh = o3d.TriangleMesh()
    mesh.vertices = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]]
    mesh.triangles = [[0, 1, 2], [0, 1, 3], [0, 2, 3], [1, 2, 3]]
    assert mesh.num_vertices() == 4
    assert mesh.num_triangles() == 4
    assert mesh.get_surface_area() > 0

    mesh.paint_uniform_color([0.2, 0.7, 0.3])
    assert mesh.has_vertex_colors()

    mesh.translate([5, 5, 5])
    center = mesh.get_center()
    assert center[0] > 5.0
    print("  PASS: TriangleMesh")

if __name__ == "__main__":
    print("Testing Open3D geometry port (mirror_bridge):")
    test_point_cloud()
    test_bounding_box()
    test_line_set()
    test_triangle_mesh()
    print("\nAll Open3D geometry tests passed!")
