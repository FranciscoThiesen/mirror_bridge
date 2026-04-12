#pragma once

#include <memory>
#include <string>
#include <vector>

struct Point {
    double x = 0.0;
    double y = 0.0;

    double length() const {
        return std::sqrt(x * x + y * y);
    }
};

struct Shape {
    std::string name = "shape";
    std::shared_ptr<Point> center;

    Shape() : center(std::make_shared<Point>()) {}

    std::shared_ptr<Point> get_center() const {
        return center;
    }

    void set_center(double x, double y) {
        center = std::make_shared<Point>(Point{x, y});
    }
};
