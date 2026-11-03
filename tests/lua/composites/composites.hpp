// Composite conversions: a smart pointer, a fixed-size array or a container
// either receives the value it was given or the assignment is refused, and
// every element type a top-level value may have, a container element may
// have too. Each comment says what used to happen in Lua and JavaScript.
#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

struct Point2 {
    double x = 0, y = 0;
};

struct Cell {
    int row = 0, col = 0;
    Cell(int r, int c) : row(r), col(c) {}
};

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct Circle : Shape {
    double r = 1;
    Circle() = default;
    explicit Circle(double radius) : r(radius) {}
    double area() const override { return 3.0 * r * r; }
};

// A smart pointer to an abstract class cannot be built from a host value, so
// only nil/null converts. Anything else used to "convert" to a null pointer:
// scene.shape = circle cleared the member and area_of(circle) received
// nullptr.
struct Scene {
    std::shared_ptr<Shape> shape;
    void set_circle(double r) { shape = std::make_shared<Circle>(r); }
    bool has_shape() const { return shape != nullptr; }
    double area_of(std::shared_ptr<Shape> s) const { return s ? s->area() : -1.0; }
};

// A std::array is filled in place from exactly N elements. It used to take
// any length and keep none of it: the member read back as uninitialised
// memory and the parameter as zeros. A std::span cannot be filled at all and
// used to arrive empty.
struct Body {
    std::array<double, 3> pos{};
    double pos_sum() const { return pos[0] + pos[1] + pos[2]; }
    double sum3(std::array<double, 3> a) const { return a[0] + a[1] + a[2]; }
    double total(std::span<const double> s) const {
        double t = 0;
        for (double v : s) t += v;
        return t;
    }
};

// Every member here but frames failed the whole module to compile: the
// container converters could not see the converters for their elements.
// frames compiled and stored garbage, because each inner std::array was a
// local that was never filled before being pushed back.
struct Collections {
    std::vector<Point2> points;
    std::array<Point2, 2> ends{};
    std::vector<std::optional<int>> maybe;
    std::vector<std::shared_ptr<Point2>> shared;
    std::vector<std::pair<int, double>> pairs;
    std::vector<bool> flags;
    std::vector<std::array<double, 3>> frames;
    std::vector<Cell> cells;   // readable; no default constructor, so not assignable

    double points_sum() const {
        double t = 0;
        for (const auto& p : points) t += p.x + p.y;
        return t;
    }
    double ends_sum() const { return ends[0].x + ends[0].y + ends[1].x + ends[1].y; }
    int maybe_sum() const {
        int t = 0;
        for (const auto& m : maybe) t += m.value_or(100);
        return t;
    }
    void fill_maybe() { maybe = {1, std::nullopt, 3}; }
    double shared_sum() const {
        double t = 0;
        for (const auto& p : shared) t += p ? p->x + p->y : 0;
        return t;
    }
    double pairs_sum() const {
        double t = 0;
        for (const auto& [k, v] : pairs) t += k * v;
        return t;
    }
    int flag_count() const {
        int n = 0;
        for (bool f : flags) n += f ? 1 : 0;
        return n;
    }
    double frames_sum() const {
        double t = 0;
        for (const auto& f : frames) t += f[0] + f[1] + f[2];
        return t;
    }
    void add_cell(int r, int c) { cells.emplace_back(r, c); }
    int cell_count() const { return static_cast<int>(cells.size()); }
};

struct Color {
    int r = 0, g = 0, b = 0;
};

// A different class whose members have Point2's names. A wrapper of it is
// still not a Point2, inside a composite as at top level.
struct Mark {
    double x = 7, y = 9;
};

// A variant and an optional ask whether a value converts to a class, and the
// answer has to be strict: a table missing a field is not that class. Only
// the lenient reader a top-level member uses would say yes to every table,
// so Choices.pc = {r = 1, g = 2, b = 3} would become Point2{0, 0}.
struct Choices {
    std::variant<Point2, Color> pc;
    std::optional<Point2> maybe_point;
    int pc_index() const { return static_cast<int>(pc.index()); }
    int pc_red() const { return pc.index() == 1 ? std::get<1>(pc).r : -1; }
    bool has_point() const { return maybe_point.has_value(); }
};

// Movable but not copyable, inside a composite: read field by field, it has
// always converted, and taking a wrapper by copy must not make it stop.
struct Node {
    int value = 0;
    std::unique_ptr<int> extra;
};

struct Owner {
    std::optional<Node> head;
    int head_value() const { return head ? head->value : -1; }
    int head_extra() const { return head && head->extra ? *head->extra : -1; }
};

// Maps inside other composites. In Lua each used to crash on its first
// element or field, once it could compile at all: the map converter pushed
// onto the stack and then read its table through a relative index.
struct Labelled {
    std::map<std::string, int> tags;
};

struct Maps {
    std::vector<std::map<std::string, int>> layers;
    std::vector<Labelled> items;
    Labelled single;

    static int sum(const std::map<std::string, int>& m) {
        int t = 0;
        for (const auto& [k, v] : m) t += v;
        return t;
    }
    int layers_total() const {
        int t = 0;
        for (const auto& m : layers) t += sum(m);
        return t;
    }
    int items_total() const {
        int t = 0;
        for (const auto& item : items) t += sum(item.tags);
        return t;
    }
    int single_total() const { return sum(single.tags); }
};
