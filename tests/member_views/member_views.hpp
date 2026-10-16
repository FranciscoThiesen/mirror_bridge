#pragma once
#include <string>
#include <vector>

namespace views {

struct Leaf {
    double x = 0.0;
    int tag = 0;
};

struct Middle {
    Leaf leaf;
    double weight = 1.0;
};

struct Base {
    Leaf origin;
};

// A member the owner declares const: writing through a view of it would get
// around what C++ said, so these keep the old copying behaviour.
struct Owner : Base {
    Middle middle;
    Leaf plain;
    const Leaf frozen{};
    std::vector<Leaf> many{Leaf{}, Leaf{}};
    std::string label = "owner";

    // Method returns stay copies: a reference out of a method cannot be shown
    // to be interior to the receiver, so there is nothing to prove here.
    Leaf& leaf_ref() { return plain; }
    Leaf leaf_copy() const { return plain; }
    double read_leaf(const Leaf& l) const { return l.x; }
};

// A four-level chain: each level's view holds the one above it, so holding
// only the deepest has to keep the whole ancestry alive.
struct Level3 { Leaf leaf; };
struct Level2 { Level3 three; };
struct Level1 { Level2 two; };

}  // namespace views
