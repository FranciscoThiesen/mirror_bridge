#pragma once
#include <cmath>

// Simple 2D Vertex class - will be bound in vertex_binding.cpp
// Named "Vertex" to avoid collision with other Point classes in tests
struct Vertex {
    double x;
    double y;

    Vertex() : x(0), y(0) {}
    Vertex(double x_, double y_) : x(x_), y(y_) {}

    // Method to test that methods work on returned Vertex objects
    double distance_from_origin() const {
        return std::sqrt(x * x + y * y);
    }

    Vertex add(const Vertex& other) const {
        return Vertex(x + other.x, y + other.y);
    }
};
