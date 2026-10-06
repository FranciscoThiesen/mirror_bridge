#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Container-shaped: begin()/end() make it iterable, size() makes it sized.
struct Bag {
    std::vector<int> items;

    void add(int v) { items.push_back(v); }
    void clear() { items.clear(); }
    std::size_t size() const { return items.size(); }
    std::vector<int>::const_iterator begin() const { return items.begin(); }
    std::vector<int>::const_iterator end() const { return items.end(); }
};

// Iterable over a non-arithmetic element type.
struct Shelf {
    std::vector<std::string> titles;

    void add(const std::string& t) { titles.push_back(t); }
    std::size_t size() const { return titles.size(); }
    std::vector<std::string>::const_iterator begin() const { return titles.begin(); }
    std::vector<std::string>::const_iterator end() const { return titles.end(); }
};

// Plain value type: the pickle / copy.deepcopy case.
struct Doc {
    std::string title;
    int pages = 0;
    std::vector<double> scores;
};

// size() without begin()/end(): "size" here is a physical dimension, not a
// count of elements. This is the class that must not acquire len() or have
// its truthiness redefined.
struct Gauge {
    double reading = 0.0;
    std::size_t size() const { return 0; }
};

// A const member cannot be restored after reconstruction, so this class is
// deliberately not picklable.
struct Frozen {
    const int serial = 7;
    int value = 0;
};

// Iterable with no size(): the iterator has no element count to guard
// against, and len() is not offered.
struct Stream {
    int first = 0;
    int last = 0;

    struct Cursor {
        int value;
        int operator*() const { return value; }
        Cursor& operator++() { ++value; return *this; }
        bool operator!=(const Cursor& other) const { return value != other.value; }
    };

    Cursor begin() const { return Cursor{first}; }
    Cursor end() const { return Cursor{last}; }
};

// Counts its own live instances, so a test can prove that an iterator keeps
// the container alive and that nothing leaks once both are gone.
struct Tracked {
    std::vector<int> items;

    Tracked() { ++live_count(); }
    Tracked(const Tracked& other) : items(other.items) { ++live_count(); }
    Tracked(Tracked&& other) noexcept : items(std::move(other.items)) { ++live_count(); }
    ~Tracked() { --live_count(); }

    static int& live_count() { static int n = 0; return n; }
    static int live() { return live_count(); }

    void add(int v) { items.push_back(v); }
    std::size_t size() const { return items.size(); }
    std::vector<int>::const_iterator begin() const { return items.begin(); }
    std::vector<int>::const_iterator end() const { return items.end(); }
};
