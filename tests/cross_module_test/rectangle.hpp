#pragma once
#include "point.hpp"

// BoundingBox class that depends on Vertex
// Will be bound in bbox_binding.cpp (separate from vertex_binding.cpp)
// Named "BoundingBox" to avoid collision with other Rectangle classes in tests
struct BoundingBox {
    Vertex top_left;
    Vertex bottom_right;

    BoundingBox() : top_left(), bottom_right() {}

    // Constructor with Vertex objects
    BoundingBox(Vertex tl, Vertex br) : top_left(tl), bottom_right(br) {}

    // Returns a Vertex - this is the key test case!
    // If cross-module type sharing works, this returns a Vertex wrapper (not a dict)
    Vertex get_top_left() const {
        return top_left;
    }

    Vertex get_bottom_right() const {
        return bottom_right;
    }

    Vertex get_center() const {
        return Vertex(
            (top_left.x + bottom_right.x) / 2.0,
            (top_left.y + bottom_right.y) / 2.0
        );
    }

    double width() const {
        return bottom_right.x - top_left.x;
    }

    double height() const {
        return bottom_right.y - top_left.y;
    }

    double area() const {
        return width() * height();
    }
};
