#pragma once

#include <string>
#include <vector>

namespace seqv {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

// Every member here exists to pin one branch of the view policy: which member
// types become a live view, which keep the copy they always had, and that a
// method returning the same C++ type as a member still copies.
struct Bag {
    std::vector<int> ints;
    std::vector<double> doubles;
    std::vector<std::string> names;
    std::vector<Point> points;

    // Writing through a view of a const member would get around what C++
    // said, so this keeps copying -- the same rule class members follow.
    const std::vector<double> frozen{1.0, 2.0};

    // vector<bool> packs its elements and hands back a proxy, so there is no
    // element to take a reference to. It keeps the list it always gave.
    std::vector<bool> flags{true, false};

    int total() const {
        int sum = 0;
        for (int v : ints) sum += v;
        return sum;
    }

    double sum_doubles() const {
        double sum = 0.0;
        for (double v : doubles) sum += v;
        return sum;
    }

    std::size_t name_count() const { return names.size(); }

    // Interiority cannot be established for a method return, so this still
    // crosses as a copy even though its type matches a member's.
    std::vector<double> doubled() const {
        std::vector<double> out;
        for (double v : doubles) out.push_back(v * 2.0);
        return out;
    }
};

}  // namespace seqv
