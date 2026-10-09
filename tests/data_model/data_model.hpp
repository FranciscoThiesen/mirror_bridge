#pragma once

#include <cstddef>
#include <iterator>
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

// A C++20 range that ends at a sentinel: end() does not return an iterator.
// Storing it as the iterator type is what used to fail the module build.
struct Countdown {
    int from = 3;

    struct Sentinel {};
    struct Tick {
        int value;
        int operator*() const { return value; }
        Tick& operator++() { --value; return *this; }
    };
    friend bool operator!=(const Tick& c, Sentinel) { return c.value > 0; }

    Tick begin() const { return Tick{from}; }
    Sentinel end() const { return Sentinel{}; }
};

// An iterator whose dereference type is not its element type, the way
// std::vector<bool>'s is. What has to reach Python is value_type: converting
// the proxy itself walks its members and hands back a dict. Spelled out here
// rather than using std::vector<bool> so the test does not depend on whether
// a standard library makes its bit-iterator members public.
struct Flags {
    int packed = 0;
    int count = 0;

    struct BitRef {
        int value;
        explicit operator bool() const { return value != 0; }
    };

    struct BitCursor {
        using iterator_category = std::input_iterator_tag;
        using value_type = bool;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = BitRef;

        int packed;
        int index;

        BitRef operator*() const { return BitRef{(packed >> index) & 1}; }
        BitCursor& operator++() { ++index; return *this; }
        bool operator!=(const BitCursor& other) const { return index != other.index; }
    };

    void add(bool bit) {
        if (bit) packed |= (1 << count);
        ++count;
    }
    std::size_t size() const { return static_cast<std::size_t>(count); }
    BitCursor begin() const { return BitCursor{packed, 0}; }
    BitCursor end() const { return BitCursor{packed, count}; }
};

// A private member is absent from the reflected visible member list, so the
// pickle state cannot carry it and this class must refuse to pickle.
class Secretive {
public:
    int visible = 1;

    void set_hidden(int v) { hidden_ = v; }
    int get_hidden() const { return hidden_; }

private:
    int hidden_ = 0;
};

// A base class's members are not in nonstatic_data_members_of(^^Derived), so
// the same refusal applies.
struct WithBase {
    int inherited = 0;
};
struct Derived : WithBase {
    int own = 0;
};

// size() that does not fit a Py_ssize_t. An unsigned count computed as
// "one less than empty" is the realistic way to get here.
struct Underflowed {
    std::vector<int> items;

    void add(int v) { items.push_back(v); }
    std::size_t size() const { return items.size() - 1; }
    std::vector<int>::const_iterator begin() const { return items.begin(); }
    std::vector<int>::const_iterator end() const { return items.end(); }
};

// Every mutation here keeps size() identical, so the element-count guard sees
// nothing and only the begin() check can catch it.
struct Sneaky {
    std::vector<int> items;

    void add(int v) { items.push_back(v); }

    void reallocate() {
        std::vector<int> fresh(items);
        fresh.reserve(items.capacity() * 4 + 64);
        items.swap(fresh);
    }

    void grow_capacity() { items.reserve(items.capacity() * 4 + 64); }

    std::size_t size() const { return items.size(); }
    std::vector<int>::const_iterator begin() const { return items.begin(); }
    std::vector<int>::const_iterator end() const { return items.end(); }
};

// operator== without a hash: CPython's slot inheritance leaves tp_hash null,
// so the type is unhashable rather than equal-but-differently-hashed.
struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};
