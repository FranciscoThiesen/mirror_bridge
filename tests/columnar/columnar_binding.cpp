// Built with -DMIRROR_BRIDGE_COLUMNAR_VECTORS by test_columnar.sh, so the
// behaviour of the returned view can be checked from Python rather than only
// the descriptor from C++.
#include "python/mirror_bridge_python.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct Point3 {
    double x = 0, y = 0, z = 0;
};

// Internal and trailing padding.
struct Tick {
    double price = 0;
    std::int32_t size = 0;
    char side = 'B';
};

// A member called f1, which is the name numpy gives an unnamed padding entry
// at index 1.
struct FNames {
    char f1 = 0;
    double x = 0;
};

// Not describable by its bytes, so it must keep returning a list.
struct Named {
    std::string label;
    double value = 0;
};

struct Source {
    std::vector<Point3> points;
    std::vector<Tick> ticks;
    std::vector<FNames> fnames;
    std::vector<Named> named;
    std::vector<double> flat;

    void fill(int n) {
        points.clear(); ticks.clear(); fnames.clear(); named.clear(); flat.clear();
        for (int i = 0; i < n; ++i) {
            points.push_back(Point3{i * 1.0, i * 2.0, i * 3.0});
            ticks.push_back(Tick{i * 0.5, i, static_cast<char>((i % 2) ? 'B' : 'S')});
            fnames.push_back(FNames{static_cast<char>(i), i * 1.5});
            named.push_back(Named{"n" + std::to_string(i), i * 1.0});
            flat.push_back(i * 1.0);
        }
    }

    std::vector<Point3> get_points() const { return points; }
    std::vector<Tick> get_ticks() const { return ticks; }
    std::vector<FNames> get_fnames() const { return fnames; }
    std::vector<Named> get_named() const { return named; }
    std::vector<double> get_flat() const { return flat; }
};

MIRROR_BRIDGE_MODULE(colv,
    mirror_bridge::bind_class<Point3>(m, "Point3");
    mirror_bridge::bind_class<Tick>(m, "Tick");
    mirror_bridge::bind_class<FNames>(m, "FNames");
    mirror_bridge::bind_class<Named>(m, "Named");
    mirror_bridge::bind_class<Source>(m, "Source");
)
